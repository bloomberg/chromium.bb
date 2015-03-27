// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_OZONE_CAST_EGL_PLATFORM_H_
#define CHROMECAST_OZONE_CAST_EGL_PLATFORM_H_

#include "ui/ozone/public/surface_factory_ozone.h"

namespace gfx {
class Size;
}

namespace chromecast {
namespace ozone {

// Interface representing all the hardware-specific elements of an Ozone
// implementation for Cast.  Supply an implementation of this interface
// to OzonePlatformCast to create a complete Ozone implementation.
class CastEglPlatform {
 public:
  typedef ui::SurfaceFactoryOzone::AddGLLibraryCallback AddGLLibraryCallback;
  typedef ui::SurfaceFactoryOzone::SetGLGetProcAddressProcCallback
      SetGLGetProcAddressProcCallback;

  virtual ~CastEglPlatform() {}

  // Default display size is used for initial display and also as a minimum
  // resolution for applications.
  virtual gfx::Size GetDefaultDisplaySize() const = 0;

  // Returns an array of EGL properties, which can be used in any EGL function
  // used to select a display configuration. Note that all properties should be
  // immediately followed by the corresponding desired value and array should be
  // terminated with EGL_NONE. Ownership of the array is not transferred to
  // caller. desired_list contains list of desired EGL properties and values.
  virtual const int32* GetEGLSurfaceProperties(const int32* desired_list) = 0;

  // Initialize/ShutdownHardware are called at most once each over the object's
  // lifetime.  Initialize will be called before creating display type or
  // window.  If Initialize fails, return false (Shutdown will still be called).
  virtual bool InitializeHardware() = 0;
  virtual void ShutdownHardware() = 0;

  // Called once after hardware successfully initialized.  Implementation needs
  // to add the EGL and GLES2 libraries through add_gl_library and also supply
  // a pointer to eglGetProcAddress (or equivalent function).
  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) = 0;

  // Create/destroy an EGLNativeDisplayType.  These may be called multiple times
  // over the object's lifetime, for example to release the display when
  // switching to an external application.  There will be at most one display
  // type at a time.
  virtual intptr_t CreateDisplayType(const gfx::Size& size) = 0;
  virtual void DestroyDisplayType(intptr_t display_type) = 0;

  // Create/destroy an EGLNativeWindow.  There will be at most one window at a
  // time, created within a valid display type.
  virtual intptr_t CreateWindow(intptr_t display_type,
                                const gfx::Size& size) = 0;
  virtual void DestroyWindow(intptr_t window) = 0;
};

}  // namespace ozone
}  // namespace chromecast

#endif  // CHROMECAST_OZONE_CAST_EGL_PLATFORM_H_
