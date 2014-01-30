// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "cc/output/software_frame_data.h"
#include "content/browser/compositor/software_output_device_ozone.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/gfx/ozone/surface_factory_ozone.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_implementation.h"

namespace {

class MockSurfaceFactoryOzone : public gfx::SurfaceFactoryOzone {
 public:
  MockSurfaceFactoryOzone() {}
  virtual ~MockSurfaceFactoryOzone() {}

  virtual HardwareState InitializeHardware() OVERRIDE {
    return SurfaceFactoryOzone::INITIALIZED;
  }

  virtual void ShutdownHardware() OVERRIDE {}
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE { return 1; }
  virtual gfx::AcceleratedWidget RealizeAcceleratedWidget(
      gfx::AcceleratedWidget w) OVERRIDE { return w; }
  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) OVERRIDE {
    return false;
  }
  virtual bool AttemptToResizeAcceleratedWidget(
      gfx::AcceleratedWidget w, const gfx::Rect& bounds) OVERRIDE {
    device_ = skia::AdoptRef(new SkBitmapDevice(SkBitmap::kARGB_8888_Config,
                                                bounds.width(),
                                                bounds.height(),
                                                true));
    canvas_ = skia::AdoptRef(new SkCanvas(device_.get()));
    return true;
  }
  virtual SkCanvas* GetCanvasForWidget(gfx::AcceleratedWidget w) OVERRIDE {
    return canvas_.get();
  }
  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider(
      gfx::AcceleratedWidget w) OVERRIDE {
    return scoped_ptr<gfx::VSyncProvider>();
  }
 private:
  skia::RefPtr<SkBitmapDevice> device_;
  skia::RefPtr<SkCanvas> canvas_;

  DISALLOW_COPY_AND_ASSIGN(MockSurfaceFactoryOzone);
};

}  // namespace

class SoftwareOutputDeviceOzoneTest : public testing::Test {
 public:
  SoftwareOutputDeviceOzoneTest();
  virtual ~SoftwareOutputDeviceOzoneTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  scoped_ptr<content::SoftwareOutputDeviceOzone> output_device_;

 private:
  scoped_ptr<ui::Compositor> compositor_;
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<gfx::SurfaceFactoryOzone> surface_factory_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceOzoneTest);
};

SoftwareOutputDeviceOzoneTest::SoftwareOutputDeviceOzoneTest() {
  message_loop_.reset(new base::MessageLoopForUI);
}

SoftwareOutputDeviceOzoneTest::~SoftwareOutputDeviceOzoneTest() {
}

void SoftwareOutputDeviceOzoneTest::SetUp() {
  ui::InitializeContextFactoryForTests(false);
  ui::Compositor::Initialize();

  surface_factory_.reset(new MockSurfaceFactoryOzone());
  gfx::SurfaceFactoryOzone::SetInstance(surface_factory_.get());

  const gfx::Size size(500, 400);
  compositor_.reset(new ui::Compositor(
      gfx::SurfaceFactoryOzone::GetInstance()->GetAcceleratedWidget()));
  compositor_->SetScaleAndSize(1.0f, size);

  output_device_.reset(new content::SoftwareOutputDeviceOzone(
      compositor_.get()));
  output_device_->Resize(size);
}

void SoftwareOutputDeviceOzoneTest::TearDown() {
  output_device_.reset();
  compositor_.reset();
  surface_factory_.reset();
  ui::TerminateContextFactoryForTests();
  ui::Compositor::Terminate();
}

TEST_F(SoftwareOutputDeviceOzoneTest, CheckClipAfterBeginPaint) {
  gfx::Rect damage(10, 10, 100, 100);
  SkCanvas* canvas = output_device_->BeginPaint(damage);

  SkIRect sk_bounds;
  canvas->getClipDeviceBounds(&sk_bounds);

  EXPECT_EQ(damage.ToString(), gfx::SkIRectToRect(sk_bounds).ToString());
}

TEST_F(SoftwareOutputDeviceOzoneTest, CheckClipAfterSecondBeginPaint) {
  gfx::Rect damage(10, 10, 100, 100);
  SkCanvas* canvas = output_device_->BeginPaint(damage);

  cc::SoftwareFrameData frame;
  output_device_->EndPaint(&frame);

  damage = gfx::Rect(100, 100, 100, 100);
  canvas = output_device_->BeginPaint(damage);
  SkIRect sk_bounds;
  canvas->getClipDeviceBounds(&sk_bounds);

  EXPECT_EQ(damage.ToString(), gfx::SkIRectToRect(sk_bounds).ToString());
}

TEST_F(SoftwareOutputDeviceOzoneTest, CheckCorrectResizeBehavior) {
  gfx::Rect damage(0, 0, 100, 100);
  gfx::Size size(200, 100);
  // Reduce size.
  output_device_->Resize(size);

  SkCanvas* canvas = output_device_->BeginPaint(damage);
  gfx::Size canvas_size(canvas->getDeviceSize().width(),
                        canvas->getDeviceSize().height());
  EXPECT_EQ(size.ToString(), canvas_size.ToString());

  size.SetSize(1000, 500);
  // Increase size.
  output_device_->Resize(size);

  canvas = output_device_->BeginPaint(damage);
  canvas_size.SetSize(canvas->getDeviceSize().width(),
                      canvas->getDeviceSize().height());
  EXPECT_EQ(size.ToString(), canvas_size.ToString());

}

TEST_F(SoftwareOutputDeviceOzoneTest, CheckCopyToBitmap) {
  const gfx::Rect area(6, 4);
  output_device_->Resize(area.size());
  SkCanvas* canvas = output_device_->BeginPaint(area);

  // Clear the background to black.
  canvas->drawColor(SK_ColorBLACK);

  cc::SoftwareFrameData frame;
  output_device_->EndPaint(&frame);

  // Draw a white rectangle.
  gfx::Rect damage(area.width() / 2, area.height() / 2);
  canvas = output_device_->BeginPaint(damage);

  canvas->drawColor(SK_ColorWHITE);

  output_device_->EndPaint(&frame);

  SkBitmap bitmap;
  output_device_->CopyToBitmap(area, &bitmap);

  SkAutoLockPixels pixel_lock(bitmap);
  // Check that the copied bitmap contains the same pixel values as what we
  // painted.
  for (int i = 0; i < area.height(); ++i) {
    for (int j = 0; j < area.width(); ++j) {
      if (j < damage.width() && i < damage.height())
        EXPECT_EQ(SK_ColorWHITE, bitmap.getColor(j, i));
      else
        EXPECT_EQ(SK_ColorBLACK, bitmap.getColor(j, i));
    }
  }
}
