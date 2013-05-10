// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

#ifdef WIN32
#undef PostMessage
// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

namespace {
const int kPthreadMutexSuccess = 0;
const char* const kPaintMethodId = "paint";
const double kInvalidPiValue = -1.0;
const int kMaxPointCount = 1000000000;  // The total number of points to draw.
const uint32_t kOpaqueColorMask = 0xff000000;  // Opaque pixels.
const uint32_t kRedMask = 0xff0000;
const uint32_t kBlueMask = 0xff;
const uint32_t kRedShift = 16;
const uint32_t kBlueShift = 0;
}  // namespace

class PiGeneratorInstance : public pp::Instance {
 public:
  explicit PiGeneratorInstance(PP_Instance instance);
  virtual ~PiGeneratorInstance();

  // Start up the ComputePi() thread.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);

  // Update the graphics context to the new size, and regenerate |pixel_buffer_|
  // to fit the new size as well.
  virtual void DidChangeView(const pp::View& view);

  // Called by the browser to handle the postMessage() call in Javascript.
  // The message in this case is expected to contain the string 'paint', and
  // if so this invokes the Paint() function.  If |var_message| is not a string
  // type, or contains something other than 'paint', this method posts an
  // invalid value for Pi (-1.0) back to the browser.
  virtual void HandleMessage(const pp::Var& var_message);

  // Return a pointer to the pixels represented by |pixel_buffer_|.  When this
  // method returns, the underlying |pixel_buffer_| object is locked.  This
  // call must have a matching UnlockPixels() or various threading errors
  // (e.g. deadlock) will occur.
  uint32_t* LockPixels();
  // Release the image lock acquired by LockPixels().
  void UnlockPixels() const;

  // Flushes its contents of |pixel_buffer_| to the 2D graphics context.  The
  // ComputePi() thread fills in |pixel_buffer_| pixels as it computes Pi.
  // This method is called by HandleMessage when a message containing 'paint'
  // is received.  Echos the current value of pi as computed by the Monte Carlo
  // method by posting the value back to the browser.
  void Paint();

  bool quit() const { return quit_; }

  // |pi_| is computed in the ComputePi() thread.
  double pi() const { return pi_; }

  int width() const {
    return pixel_buffer_ ? pixel_buffer_->size().width() : 0;
  }
  int height() const {
    return pixel_buffer_ ? pixel_buffer_->size().height() : 0;
  }

  // Indicate whether a flush is pending.  This can only be called from the
  // main thread; it is not thread safe.
  bool flush_pending() const { return flush_pending_; }
  void set_flush_pending(bool flag) { flush_pending_ = flag; }

 private:
  // Create and initialize the 2D context used for drawing.
  void CreateContext(const pp::Size& size, float device_scale);
  // Destroy the 2D drawing context.
  void DestroyContext();
  // Push the pixels to the browser, then attempt to flush the 2D context.  If
  // there is a pending flush on the 2D context, then update the pixels only
  // and do not flush.
  void FlushPixelBuffer();

  void FlushCallback(int32_t result);

  bool IsContextValid() const { return graphics_2d_context_ != NULL; }

  pp::CompletionCallbackFactory<PiGeneratorInstance> callback_factory_;
  mutable pthread_mutex_t pixel_buffer_mutex_;
  pp::Graphics2D* graphics_2d_context_;
  pp::ImageData* pixel_buffer_;
  bool flush_pending_;
  bool quit_;
  pthread_t compute_pi_thread_;
  int thread_create_result_;
  double pi_;
  float device_scale_;

  // ComputePi() estimates Pi using Monte Carlo method and it is executed by a
  // separate thread created in SetWindow(). ComputePi() puts kMaxPointCount
  // points inside the square whose length of each side is 1.0, and calculates
  // the ratio of the number of points put inside the inscribed quadrant divided
  // by the total number of random points to get Pi/4.
  static void* ComputePi(void* param);
};

// A small helper RAII class that implements a scoped pthread_mutex lock.
class ScopedMutexLock {
 public:
  explicit ScopedMutexLock(pthread_mutex_t* mutex) : mutex_(mutex) {
    if (pthread_mutex_lock(mutex_) != kPthreadMutexSuccess) {
      mutex_ = NULL;
    }
  }
  ~ScopedMutexLock() {
    if (mutex_)
      pthread_mutex_unlock(mutex_);
  }
  bool is_valid() const { return mutex_ != NULL; }

 private:
  pthread_mutex_t* mutex_;  // Weak reference.
};

// A small helper RAII class used to acquire and release the pixel lock.
class ScopedPixelLock {
 public:
  explicit ScopedPixelLock(PiGeneratorInstance* image_owner)
      : image_owner_(image_owner), pixels_(image_owner->LockPixels()) {}

  ~ScopedPixelLock() {
    pixels_ = NULL;
    image_owner_->UnlockPixels();
  }

  uint32_t* pixels() const { return pixels_; }

 private:
  PiGeneratorInstance* image_owner_;  // Weak reference.
  uint32_t* pixels_;                  // Weak reference.
};

PiGeneratorInstance::PiGeneratorInstance(PP_Instance instance)
    : pp::Instance(instance),
      callback_factory_(this),
      graphics_2d_context_(NULL),
      pixel_buffer_(NULL),
      flush_pending_(false),
      quit_(false),
      thread_create_result_(0),
      pi_(0.0),
      device_scale_(1.0) {
  pthread_mutex_init(&pixel_buffer_mutex_, NULL);
}

PiGeneratorInstance::~PiGeneratorInstance() {
  quit_ = true;
  if (thread_create_result_ == 0) {
    pthread_join(compute_pi_thread_, NULL);
  }
  DestroyContext();
  // The ComputePi() thread should be gone by now, so there is no need to
  // acquire the mutex for |pixel_buffer_|.
  delete pixel_buffer_;
  pthread_mutex_destroy(&pixel_buffer_mutex_);
}

void PiGeneratorInstance::DidChangeView(const pp::View& view) {
  pp::Size size = view.GetRect().size();
  float device_scale = view.GetDeviceScale();
  size.set_width(static_cast<int>(size.width() * device_scale));
  size.set_height(static_cast<int>(size.height() * device_scale));
  if (pixel_buffer_ && size == pixel_buffer_->size() &&
      device_scale == device_scale_)
    return;  // Size and scale didn't change, no need to update anything.

  // Create a new device context with the new size and scale.
  DestroyContext();
  device_scale_ = device_scale;
  CreateContext(size, device_scale_);
  // Delete the old pixel buffer and create a new one.
  ScopedMutexLock scoped_mutex(&pixel_buffer_mutex_);
  delete pixel_buffer_;
  pixel_buffer_ = NULL;
  if (graphics_2d_context_ != NULL) {
    pixel_buffer_ = new pp::ImageData(this,
                                      PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                                      graphics_2d_context_->size(),
                                      false);
  }
}

bool PiGeneratorInstance::Init(uint32_t argc,
                               const char* argn[],
                               const char* argv[]) {
  thread_create_result_ =
      pthread_create(&compute_pi_thread_, NULL, ComputePi, this);
  return thread_create_result_ == 0;
}

uint32_t* PiGeneratorInstance::LockPixels() {
  void* pixels = NULL;
  // Do not use a ScopedMutexLock here, since the lock needs to be held until
  // the matching UnlockPixels() call.
  if (pthread_mutex_lock(&pixel_buffer_mutex_) == kPthreadMutexSuccess) {
    if (pixel_buffer_ != NULL && !pixel_buffer_->is_null()) {
      pixels = pixel_buffer_->data();
    }
  }
  return reinterpret_cast<uint32_t*>(pixels);
}

void PiGeneratorInstance::HandleMessage(const pp::Var& var_message) {
  if (!var_message.is_string()) {
    PostMessage(pp::Var(kInvalidPiValue));
  }
  std::string message = var_message.AsString();
  if (message == kPaintMethodId) {
    Paint();
  } else {
    PostMessage(pp::Var(kInvalidPiValue));
  }
}

void PiGeneratorInstance::UnlockPixels() const {
  pthread_mutex_unlock(&pixel_buffer_mutex_);
}

void PiGeneratorInstance::Paint() {
  ScopedMutexLock scoped_mutex(&pixel_buffer_mutex_);
  if (!scoped_mutex.is_valid()) {
    return;
  }
  FlushPixelBuffer();
  // Post the current estimate of Pi back to the browser.
  pp::Var pi_estimate(pi());
  // Paint() is called on the main thread, so no need for CallOnMainThread()
  // here.  It's OK to just post the message.
  PostMessage(pi_estimate);
}

void PiGeneratorInstance::CreateContext(const pp::Size& size,
                                        float device_scale) {
  ScopedMutexLock scoped_mutex(&pixel_buffer_mutex_);
  if (!scoped_mutex.is_valid()) {
    return;
  }
  if (IsContextValid())
    return;
  graphics_2d_context_ = new pp::Graphics2D(this, size, false);
  // Scale the contents of the graphics context down by the inverse of the
  // device scale. This makes each pixel in the context represent a single
  // physical pixel on the device when running on high-DPI displays.
  // See pp::Graphics2D::SetScale for more details.
  graphics_2d_context_->SetScale(1.0 / device_scale);
  if (!BindGraphics(*graphics_2d_context_)) {
    printf("Couldn't bind the device context\n");
  }
}

void PiGeneratorInstance::DestroyContext() {
  ScopedMutexLock scoped_mutex(&pixel_buffer_mutex_);
  if (!scoped_mutex.is_valid()) {
    return;
  }
  if (!IsContextValid())
    return;
  delete graphics_2d_context_;
  graphics_2d_context_ = NULL;
}

void PiGeneratorInstance::FlushPixelBuffer() {
  if (!IsContextValid())
    return;
  // Note that the pixel lock is held while the buffer is copied into the
  // device context and then flushed.
  graphics_2d_context_->PaintImageData(*pixel_buffer_, pp::Point());
  if (flush_pending())
    return;
  set_flush_pending(true);
  graphics_2d_context_->Flush(
      callback_factory_.NewCallback(&PiGeneratorInstance::FlushCallback));
}

// This is called by the browser when the 2D context has been flushed to the
// browser window.
void PiGeneratorInstance::FlushCallback(int32_t result) {
  set_flush_pending(false);
}

void* PiGeneratorInstance::ComputePi(void* param) {
  int count = 0;  // The number of points put inside the inscribed quadrant.
  unsigned int seed = 1;
  srand(seed);

  PiGeneratorInstance* pi_generator = static_cast<PiGeneratorInstance*>(param);
  for (int i = 1; i <= kMaxPointCount && !pi_generator->quit(); ++i) {
    ScopedPixelLock scoped_pixel_lock(pi_generator);
    uint32_t* pixel_bits = scoped_pixel_lock.pixels();
    if (pixel_bits == NULL) {
      // Note that if the pixel buffer never gets initialized, this won't ever
      // paint anything.  Which is probably the right thing to do.  Also, this
      // clause means that the image will not get the very first few Pi dots,
      // since it's possible that this thread starts before the pixel buffer is
      // initialized.
      continue;
    }
    double x = static_cast<double>(rand()) / RAND_MAX;
    double y = static_cast<double>(rand()) / RAND_MAX;
    double distance = sqrt(x * x + y * y);
    int px = x * pi_generator->width();
    int py = (1.0 - y) * pi_generator->height();
    uint32_t color = pixel_bits[pi_generator->width() * py + px];
    if (distance < 1.0) {
      // Set color to blue.
      ++count;
      pi_generator->pi_ = 4.0 * count / i;
      color += 4 << kBlueShift;
      color &= kBlueMask;
    } else {
      // Set color to red.
      color += 4 << kRedShift;
      color &= kRedMask;
    }
    pixel_bits[pi_generator->width() * py + px] = color | kOpaqueColorMask;
  }
  return 0;
}

class PiGeneratorModule : public pp::Module {
 public:
  PiGeneratorModule() : pp::Module() {}
  virtual ~PiGeneratorModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new PiGeneratorInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new PiGeneratorModule(); }
}  // namespace pp
