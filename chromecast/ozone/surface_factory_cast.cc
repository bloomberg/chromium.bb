// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ozone/surface_factory_cast.h"

#include "base/callback_helpers.h"
#include "chromecast/ozone/cast_egl_platform.h"
#include "chromecast/ozone/surface_ozone_egl_cast.h"

namespace chromecast {
namespace ozone {

SurfaceFactoryCast::SurfaceFactoryCast(scoped_ptr<CastEglPlatform> egl_platform)
    : state_(kUninitialized),
      destroy_window_pending_state_(kNoDestroyPending),
      display_type_(0),
      window_(0),
      default_display_size_(egl_platform->GetDefaultDisplaySize()),
      display_size_(default_display_size_),
      new_display_size_(default_display_size_),
      egl_platform_(egl_platform.Pass()) {
}

SurfaceFactoryCast::~SurfaceFactoryCast() {
  DestroyDisplayTypeAndWindow();
}

void SurfaceFactoryCast::InitializeHardware() {
  if (state_ == kInitialized) {
    return;
  }
  CHECK_EQ(state_, kUninitialized);

  if (egl_platform_->InitializeHardware()) {
    state_ = kInitialized;
  } else {
    ShutdownHardware();
    state_ = kFailed;
  }
}

void SurfaceFactoryCast::ShutdownHardware() {
  DestroyDisplayTypeAndWindow();

  egl_platform_->ShutdownHardware();

  state_ = kUninitialized;
}

intptr_t SurfaceFactoryCast::GetNativeDisplay() {
  CreateDisplayTypeAndWindowIfNeeded();
  return reinterpret_cast<intptr_t>(display_type_);
}

void SurfaceFactoryCast::CreateDisplayTypeAndWindowIfNeeded() {
  if (state_ == kUninitialized) {
    InitializeHardware();
  }
  if (new_display_size_ != display_size_) {
    DestroyDisplayTypeAndWindow();
    display_size_ = new_display_size_;
  }
  DCHECK_EQ(state_, kInitialized);
  if (!display_type_) {
    display_type_ = egl_platform_->CreateDisplayType(display_size_);
    if (display_type_) {
      window_ = egl_platform_->CreateWindow(display_type_, display_size_);
      if (!window_) {
        DestroyDisplayTypeAndWindow();
        state_ = kFailed;
        LOG(FATAL) << "Create EGLNativeWindowType(" << display_size_.ToString()
                   << ") failed.";
      }
    } else {
      state_ = kFailed;
      LOG(FATAL) << "Create EGLNativeDisplayType(" << display_size_.ToString()
                 << ") failed.";
    }
  }
}

intptr_t SurfaceFactoryCast::GetNativeWindow() {
  CreateDisplayTypeAndWindowIfNeeded();
  return window_;
}

bool SurfaceFactoryCast::ResizeDisplay(gfx::Size size) {
  // set size to at least 1280x720 even if passed 1x1
  size.SetToMax(default_display_size_);
  if (display_type_ && size != display_size_) {
    DestroyDisplayTypeAndWindow();
  }
  display_size_ = size;
  return true;
}

void SurfaceFactoryCast::DestroyDisplayTypeAndWindow() {
  if (window_) {
    egl_platform_->DestroyWindow(window_);
    window_ = 0;
  }
  if (display_type_) {
    egl_platform_->DestroyDisplayType(display_type_);
    display_type_ = 0;
  }
}

scoped_ptr<ui::SurfaceOzoneEGL> SurfaceFactoryCast::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  new_display_size_ = gfx::Size(widget >> 16, widget & 0xFFFF);
  new_display_size_.SetToMax(default_display_size_);
  destroy_window_pending_state_ = kSurfaceExists;
  SendRelinquishResponse();
  return make_scoped_ptr<ui::SurfaceOzoneEGL>(new SurfaceOzoneEglCast(this));
}

void SurfaceFactoryCast::SetToRelinquishDisplay(const base::Closure& callback) {
  // This is called in response to a RelinquishDisplay message from the
  // browser task. This call may come before or after the display surface
  // is actually destroyed.
  relinquish_display_callback_ = callback;
  switch (destroy_window_pending_state_) {
    case kNoDestroyPending:
    case kSurfaceDestroyedRecently:
      DestroyDisplayTypeAndWindow();
      SendRelinquishResponse();
      destroy_window_pending_state_ = kNoDestroyPending;
      break;
    case kSurfaceExists:
      destroy_window_pending_state_ = kWindowDestroyPending;
      break;
    case kWindowDestroyPending:
      break;
    default:
      NOTREACHED();
  }
}

void SurfaceFactoryCast::ChildDestroyed() {
  if (destroy_window_pending_state_ == kWindowDestroyPending) {
    DestroyDisplayTypeAndWindow();
    SendRelinquishResponse();
    destroy_window_pending_state_ = kNoDestroyPending;
  } else {
    destroy_window_pending_state_ = kSurfaceDestroyedRecently;
  }
}

void SurfaceFactoryCast::SendRelinquishResponse() {
  if (!relinquish_display_callback_.is_null()) {
    base::ResetAndReturn(&relinquish_display_callback_).Run();
  }
}

const int32* SurfaceFactoryCast::GetEGLSurfaceProperties(
    const int32* desired_list) {
  return egl_platform_->GetEGLSurfaceProperties(desired_list);
}

bool SurfaceFactoryCast::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  if (state_ != kInitialized) {
    InitializeHardware();
    if (state_ != kInitialized) {
      return false;
    }
  }

  return egl_platform_->LoadEGLGLES2Bindings(add_gl_library,
                                             set_gl_get_proc_address);
}

}  // namespace ozone
}  // namespace chromecast
