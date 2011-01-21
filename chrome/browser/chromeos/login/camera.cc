// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/camera.h"

#include <stdlib.h>
#include <fcntl.h>  // low-level i/o
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>  // for videodev2.h
#include <linux/videodev2.h>

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "chrome/browser/browser_thread.h"
#include "gfx/size.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"

namespace chromeos {

namespace {

// Logs errno number and its text.
void log_errno(const std::string& message) {
  LOG(ERROR) << message << " errno: " << errno << ", " << strerror(errno);
}

// Helpful wrapper around ioctl that retries it upon failure in cases when
// this is appropriate.
int xioctl(int fd, int request, void* arg) {
  int r;
  do {
    r = ioctl(fd, request, arg);
  } while (r == -1 && errno == EINTR);
  return r;
}

// Clips integer value to 1 byte boundaries. Saturates the result on
// overflow or underflow.
uint8_t clip_to_byte(int value) {
  if (value > 255)
    value = 255;
  if (value < 0)
    value = 0;
  return static_cast<uint8_t>(value);
}

// Converts color from YUV colorspace to RGB. Returns the result in Skia
// format suitable for use with SkBitmap. For the formula see
// "Converting between YUV and RGB" article on MSDN:
// http://msdn.microsoft.com/en-us/library/ms893078.aspx
uint32_t convert_yuv_to_rgba(int y, int u, int v) {
  int c = y - 16;
  int d = u - 128;
  int e = v - 128;
  uint8_t r = clip_to_byte((298 * c + 409 * e + 128) >> 8);
  uint8_t g = clip_to_byte((298 * c - 100 * d - 208 * e + 128) >> 8);
  uint8_t b = clip_to_byte((298 * c + 516 * d + 128) >> 8);
  return SkPackARGB32(255U, r, g, b);
}

// Enumerates available frame sizes for specified pixel format and picks up the
// best one to set for the desired image resolution.
gfx::Size get_best_frame_size(int fd,
                              int pixel_format,
                              int desired_width,
                              int desired_height) {
  v4l2_frmsizeenum size = {};
  size.index = 0;
  size.pixel_format = pixel_format;
  std::vector<gfx::Size> sizes;
  int r = xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &size);
  while (r != -1) {
    if (size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
      sizes.push_back(gfx::Size(size.discrete.width, size.discrete.height));
    }
    ++size.index;
    r = xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &size);
  }
  if (sizes.empty()) {
    NOTREACHED();
    return gfx::Size(desired_width, desired_height);
  }
  for (size_t i = 0; i < sizes.size(); ++i) {
    if (sizes[i].width() >= desired_width &&
        sizes[i].height() >= desired_height)
      return sizes[i];
  }
  // If higher resolution is not available, choose the highest available.
  size_t max_size_index = 0;
  int max_area = sizes[0].GetArea();
  for (size_t i = 1; i < sizes.size(); ++i) {
    if (sizes[i].GetArea() > max_area) {
      max_size_index = i;
      max_area = sizes[i].GetArea();
    }
  }
  return sizes[max_size_index];
}

// Default camera device name.
const char kDeviceName[] = "/dev/video0";
// Default width of each frame received from the camera.
const int kFrameWidth = 640;
// Default height of each frame received from the camera.
const int kFrameHeight = 480;
// Number of buffers to request from the device.
const int kRequestBuffersCount = 4;
// Timeout for select() call in microseconds.
const long kSelectTimeout = 1 * base::Time::kMicrosecondsPerSecond;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// Camera, public members:

Camera::Camera(Delegate* delegate, base::Thread* thread, bool mirrored)
    : delegate_(delegate),
      thread_(thread),
      device_name_(kDeviceName),
      device_descriptor_(-1),
      is_capturing_(false),
      desired_width_(kFrameWidth),
      desired_height_(kFrameHeight),
      frame_width_(kFrameWidth),
      frame_height_(kFrameHeight),
      mirrored_(mirrored) {
}

Camera::~Camera() {
  DCHECK_EQ(-1, device_descriptor_) << "Don't forget to uninitialize camera.";
}

void Camera::ReportFailure() {
  DCHECK(IsOnCameraThread());
  if (device_descriptor_ == -1) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(this,
                          &Camera::OnInitializeFailure));
  } else if (!is_capturing_) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(this,
                          &Camera::OnStartCapturingFailure));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(this,
                          &Camera::OnCaptureFailure));
  }
}

void Camera::Initialize(int desired_width, int desired_height) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PostCameraTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &Camera::DoInitialize,
                        desired_width,
                        desired_height));
}

void Camera::DoInitialize(int desired_width, int desired_height) {
  DCHECK(IsOnCameraThread());

  if (device_descriptor_ != -1) {
    LOG(WARNING) << "Camera is initialized already.";
    return;
  }

  int fd = OpenDevice(device_name_.c_str());
  if (fd == -1) {
    ReportFailure();
    return;
  }

  v4l2_capability cap;
  if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
    if (errno == EINVAL)
      LOG(ERROR) << device_name_ << " is no V4L2 device";
    else
      log_errno("VIDIOC_QUERYCAP failed.");
    ReportFailure();
    return;
  }
  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    LOG(ERROR) << device_name_ << " is no video capture device";
    ReportFailure();
    return;
  }
  if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
    LOG(ERROR) << device_name_ << " does not support streaming i/o";
    ReportFailure();
    return;
  }

  gfx::Size frame_size = get_best_frame_size(fd,
                                             V4L2_PIX_FMT_YUYV,
                                             desired_width,
                                             desired_height);
  v4l2_format format = {};
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  format.fmt.pix.width = frame_size.width();
  format.fmt.pix.height = frame_size.height();
  format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  format.fmt.pix.field = V4L2_FIELD_INTERLACED;
  if (xioctl(fd, VIDIOC_S_FMT, &format) == -1) {
    LOG(ERROR) << "VIDIOC_S_FMT failed.";
    ReportFailure();
    return;
  }

  if (!InitializeReadingMode(fd)) {
    ReportFailure();
    return;
  }

  device_descriptor_ = fd;
  frame_width_ = frame_size.width();
  frame_height_ = frame_size.height();
  desired_width_ = desired_width;
  desired_height_ = desired_height;
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(this, &Camera::OnInitializeSuccess));
}

void Camera::Uninitialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PostCameraTask(FROM_HERE, NewRunnableMethod(this, &Camera::DoUninitialize));
}

void Camera::DoUninitialize() {
  DCHECK(IsOnCameraThread());
  if (device_descriptor_ == -1) {
    LOG(WARNING) << "Calling uninitialize for uninitialized camera.";
    return;
  }
  DoStopCapturing();
  UnmapVideoBuffers();
  if (close(device_descriptor_) == -1)
    log_errno("Closing the device failed.");
  device_descriptor_ = -1;
}

void Camera::StartCapturing() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PostCameraTask(FROM_HERE,
                 NewRunnableMethod(this, &Camera::DoStartCapturing));
}

void Camera::DoStartCapturing() {
  DCHECK(IsOnCameraThread());
  if (is_capturing_) {
    LOG(WARNING) << "Capturing is already started.";
    return;
  }

  for (size_t i = 0; i < buffers_.size(); ++i) {
    v4l2_buffer buffer = {};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = i;
    if (xioctl(device_descriptor_, VIDIOC_QBUF, &buffer) == -1) {
      log_errno("VIDIOC_QBUF failed.");
      ReportFailure();
      return;
    }
  }
  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (xioctl(device_descriptor_, VIDIOC_STREAMON, &type) == -1) {
    log_errno("VIDIOC_STREAMON failed.");
    ReportFailure();
    return;
  }
  // No need to post DidProcessCameraThreadMethod() as this method is
  // being posted instead.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(this,
                        &Camera::OnStartCapturingSuccess));
  is_capturing_ = true;
  PostCameraTask(FROM_HERE,
                 NewRunnableMethod(this, &Camera::OnCapture));
}

void Camera::StopCapturing() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PostCameraTask(FROM_HERE,
                 NewRunnableMethod(this, &Camera::DoStopCapturing));
}

void Camera::DoStopCapturing() {
  DCHECK(IsOnCameraThread());
  if (!is_capturing_) {
    LOG(WARNING) << "Calling StopCapturing when capturing is not started.";
    return;
  }
  // OnCapture must exit if this flag is not set.
  is_capturing_ = false;
  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (xioctl(device_descriptor_, VIDIOC_STREAMOFF, &type) == -1)
    log_errno("VIDIOC_STREAMOFF failed.");
}

void Camera::GetFrame(SkBitmap* frame) {
  base::AutoLock lock(image_lock_);
  frame->swap(frame_image_);
}

///////////////////////////////////////////////////////////////////////////////
// Camera, private members:

int Camera::OpenDevice(const char* device_name) const {
  DCHECK(IsOnCameraThread());
  struct stat st;
  if (stat(device_name, &st) == -1) {
    log_errno(base::StringPrintf("Cannot identify %s", device_name));
    return -1;
  }
  if (!S_ISCHR(st.st_mode)) {
    LOG(ERROR) << device_name << "is not adevice";
    return -1;
  }
  int fd = open(device_name, O_RDWR | O_NONBLOCK, 0);
  if (fd == -1) {
    log_errno(base::StringPrintf("Cannot open %s", device_name));
    return -1;
  }
  return fd;
}

bool Camera::InitializeReadingMode(int fd) {
  DCHECK(IsOnCameraThread());
  v4l2_requestbuffers req;
  req.count = kRequestBuffersCount;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
    if (errno == EINVAL)
      LOG(ERROR) << device_name_ << " does not support memory mapping.";
    else
      log_errno("VIDIOC_REQBUFS failed.");
    return false;
  }
  if (req.count < 2U) {
    LOG(ERROR) << "Insufficient buffer memory on " << device_name_;
    return false;
  }
  for (unsigned i = 0; i < req.count; ++i) {
    v4l2_buffer buffer = {};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = i;
    if (xioctl(fd, VIDIOC_QUERYBUF, &buffer) == -1) {
      log_errno("VIDIOC_QUERYBUF failed.");
      return false;
    }
    VideoBuffer video_buffer;
    video_buffer.length = buffer.length;
    video_buffer.start = mmap(
        NULL,  // Start anywhere.
        buffer.length,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        buffer.m.offset);
    if (video_buffer.start == MAP_FAILED) {
      log_errno("mmap() failed.");
      UnmapVideoBuffers();
      return false;
    }
    buffers_.push_back(video_buffer);
  }
  return true;
}

void Camera::UnmapVideoBuffers() {
  DCHECK(IsOnCameraThread());
  for (size_t i = 0; i < buffers_.size(); ++i) {
    if (munmap(buffers_[i].start, buffers_[i].length) == -1)
      log_errno("munmap failed.");
  }
}

void Camera::OnCapture() {
  DCHECK(IsOnCameraThread());
  if (!is_capturing_)
    return;

  do {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(device_descriptor_, &fds);

    timeval tv = {};
    tv.tv_sec = kSelectTimeout / base::Time::kMicrosecondsPerSecond;
    tv.tv_usec = kSelectTimeout % base::Time::kMicrosecondsPerSecond;

    int result = select(device_descriptor_ + 1, &fds, NULL, NULL, &tv);
    if (result == -1) {
      if (errno == EINTR)
        continue;
      log_errno("select() failed.");
      ReportFailure();
      break;
    }
    if (result == 0) {
      LOG(ERROR) << "select() timeout.";
      ReportFailure();
      break;
    }
    // EAGAIN - continue select loop.
  } while (!ReadFrame());

  PostCameraTask(FROM_HERE,
                 NewRunnableMethod(this, &Camera::OnCapture));
}

bool Camera::ReadFrame() {
  DCHECK(IsOnCameraThread());
  v4l2_buffer buffer = {};
  buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buffer.memory = V4L2_MEMORY_MMAP;
  if (xioctl(device_descriptor_, VIDIOC_DQBUF, &buffer) == -1) {
    // Return false only in this case to try again.
    if (errno == EAGAIN)
      return false;

    log_errno("VIDIOC_DQBUF failed.");
    ReportFailure();
    return true;
  }
  if (buffer.index >= buffers_.size()) {
    LOG(ERROR) << "Index of buffer is out of range.";
    ReportFailure();
    return true;
  }
  ProcessImage(buffers_[buffer.index].start);
  if (xioctl(device_descriptor_, VIDIOC_QBUF, &buffer) == -1)
    log_errno("VIDIOC_QBUF failed.");
  return true;
}

void Camera::ProcessImage(void* data) {
  DCHECK(IsOnCameraThread());
  // If desired resolution is higher than available, we crop the available
  // image to get the same aspect ratio and scale the result.
  int desired_width = desired_width_;
  int desired_height = desired_height_;
  if (desired_width > frame_width_ || desired_height > frame_height_) {
    // Compare aspect ratios of the desired and available images.
    // The same as desired_width / desired_height > frame_width / frame_height.
    if (desired_width_ * frame_height_ > frame_width_ * desired_height_) {
      desired_width = frame_width_;
      desired_height = (desired_height_ * frame_width_) / desired_width_;
    } else {
      desired_width = (desired_width_ * frame_height_) / desired_height_;
      desired_height = frame_height_;
    }
  }
  SkBitmap image;
  int crop_left = (frame_width_ - desired_width) / 2;
  int crop_right = frame_width_ - crop_left - desired_width;
  int crop_top = (frame_height_ - desired_height_) / 2;
  image.setConfig(SkBitmap::kARGB_8888_Config, desired_width, desired_height);
  image.allocPixels();
  {
    SkAutoLockPixels lock_image(image);
    // We should reflect the image from the Y axis depending on the value of
    // |mirrored_|. Hence variable increments and origin point.
    int dst_x_origin = 0;
    int dst_x_increment = 1;
    int dst_y_increment = 0;
    if (mirrored_) {
      dst_x_origin = image.width() - 1;
      dst_x_increment = -1;
      dst_y_increment = 2 * image.width();
    }
    uint32_t* dst = image.getAddr32(dst_x_origin, 0);

    uint32_t* src = reinterpret_cast<uint32_t*>(data) +
                    crop_top * (frame_width_ / 2);
    for (int y = 0; y < image.height(); ++y) {
      src += crop_left / 2;
      for (int x = 0; x < image.width(); x += 2) {
        uint32_t yuyv = *src++;
        uint8_t y0 = yuyv & 0xFF;
        uint8_t u = (yuyv >> 8) & 0xFF;
        uint8_t y1 = (yuyv >> 16) & 0xFF;
        uint8_t v = (yuyv >> 24) & 0xFF;
        *dst = convert_yuv_to_rgba(y0, u, v);
        dst += dst_x_increment;
        *dst = convert_yuv_to_rgba(y1, u, v);
        dst += dst_x_increment;
      }
      dst += dst_y_increment;
      src += crop_right / 2;
    }
  }
  if (image.width() < desired_width_ || image.height() < desired_height_) {
    image = skia::ImageOperations::Resize(
        image,
        skia::ImageOperations::RESIZE_LANCZOS3,
        desired_width_,
        desired_height_);
  }
  image.setIsOpaque(true);
  {
    base::AutoLock lock(image_lock_);
    frame_image_.swap(image);
  }
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(this, &Camera::OnCaptureSuccess));
}

void Camera::OnInitializeSuccess() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (delegate_)
    delegate_->OnInitializeSuccess();
}

void Camera::OnInitializeFailure() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (delegate_)
    delegate_->OnInitializeFailure();
}

void Camera::OnStartCapturingSuccess() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (delegate_)
    delegate_->OnStartCapturingSuccess();
}

void Camera::OnStartCapturingFailure() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (delegate_)
    delegate_->OnStartCapturingFailure();
}

void Camera::OnCaptureSuccess() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (delegate_)
    delegate_->OnCaptureSuccess();
}

void Camera::OnCaptureFailure() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (delegate_)
    delegate_->OnCaptureFailure();
}

bool Camera::IsOnCameraThread() const {
  base::AutoLock lock(thread_lock_);
  return thread_ && MessageLoop::current() == thread_->message_loop();
}

void Camera::PostCameraTask(const tracked_objects::Location& from_here,
                            Task* task) {
  base::AutoLock lock(thread_lock_);
  if (!thread_)
    return;
  DCHECK(thread_->IsRunning());
  thread_->message_loop()->PostTask(from_here, task);
}

}  // namespace chromeos
