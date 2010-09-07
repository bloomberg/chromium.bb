// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_info_collector.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/sys_string_conversions.h"
#include "app/gfx/gl/gl_context.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <OpenGL/gl.h>

namespace gpu_info_collector {

static CFTypeRef SearchPortForProperty(io_registry_entry_t dspPort,
                                       CFStringRef propertyName) {
  return IORegistryEntrySearchCFProperty(dspPort,
                                         kIOServicePlane,
                                         propertyName,
                                         kCFAllocatorDefault,
                                         kIORegistryIterateRecursively |
                                         kIORegistryIterateParents);
}

static void CFReleaseIf(CFTypeRef type_ref) {
  if (type_ref)
    CFRelease(type_ref);
}

static UInt32 IntValueOfCFData(CFDataRef data_ref) {
  UInt32 value = 0;

  if (data_ref) {
    const UInt32 *value_pointer =
        reinterpret_cast<const UInt32*>(CFDataGetBytePtr(data_ref));
    if (value_pointer != NULL)
      value = *value_pointer;
  }

  return value;
}

static void CollectVideoCardInfo(CGDirectDisplayID displayID,
                                 int *vendorID,
                                 int *deviceID) {
  io_registry_entry_t dspPort = CGDisplayIOServicePort(displayID);

  CFTypeRef vendorIDRef = SearchPortForProperty(dspPort, CFSTR("vendor-id"));
  if (vendorID) *vendorID = IntValueOfCFData((CFDataRef)vendorIDRef);

  CFTypeRef deviceIDRef = SearchPortForProperty(dspPort, CFSTR("device-id"));
  if (deviceID) *deviceID = IntValueOfCFData((CFDataRef)deviceIDRef);

  CFReleaseIf(vendorIDRef);
  CFReleaseIf(deviceIDRef);
}

// Return a pointer to the last character with value c in string s.
// Returns NULL if c is not found.
static char* FindLastChar(char *s, char c) {
  char *s_found = NULL;

  while (*s != '\0') {
    if (*s == c)
      s_found = s;
    s++;
  }
  return s_found;
}

// Gets the numeric HLSL version.
// You pass it the current GL major version, to give it a hint where to look.
static int GetShaderNumericVersion(int gl_major_version) {
  int gl_hlsl_major = 0, gl_hlsl_minor = 0;
  int shader_version = 0;

  if (gl_major_version == 1) {
    char *gl_extensions_string = (char*)glGetString(GL_EXTENSIONS);
    if (strstr(gl_extensions_string, "GL_ARB_shading_language_100")) {
      gl_hlsl_major = 1;
      gl_hlsl_minor = 0;
    }
  } else if (gl_major_version > 1) {
    char *glsl_version_string = (char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    if (glsl_version_string)
      sscanf(glsl_version_string, "%u.%u", &gl_hlsl_major, &gl_hlsl_minor);
  }

  shader_version = (gl_hlsl_major << 8) | (gl_hlsl_minor & 0xFF);
  return shader_version;
}


static std::wstring CStringToWString(const char *s) {
  base::StringPiece sp(s);
  return base::SysUTF8ToWide(sp);
}


// Returns the driver version string as its value, and also returns the
// gl version and shader language version by setting the arguments pointed to.
static std::wstring CollectGLInfo(int *out_gl_version,
                                  int *out_shader_version) {
  int gl_major = 0, gl_minor = 0;
  char *gl_version_string = NULL;
  std::wstring driver_version;

  gl_version_string = (char*)glGetString(GL_VERSION);
  sscanf(gl_version_string, "%u.%u",  &gl_major, &gl_minor);

  *out_gl_version = (gl_major << 8) | (gl_minor & 0xFF);
  *out_shader_version = GetShaderNumericVersion(gl_major);

  // Extract the OpenGL driver version string from the GL_VERSION string.
  // Mac OpenGL drivers have the driver version
  // at the end of the gl version string preceded by a dash.
  // Use some jiggery-pokery to turn that utf8 string into a std::wstring.
  char *s = FindLastChar(gl_version_string, '-');
  if (s)
    driver_version = CStringToWString(s + 1);

  return driver_version;
}


bool CollectGraphicsInfo(GPUInfo* gpu_info) {
  // Video Card data.
  int vendor_id = 0, device_id = 0;
  // OpenGL data.
  std::wstring driver_version = L"";
  int gl_version = 0, shader_version = 0;

  CollectVideoCardInfo(kCGDirectMainDisplay, &vendor_id, &device_id);

  // Temporarily make an offscreen GL context so we can gather info from it.
  if (gfx::GLContext::InitializeOneOff()) {
    scoped_ptr<gfx::GLContext> ctx(
        gfx::GLContext::CreateOffscreenGLContext(NULL));
    if (ctx.get()) {
      if (ctx->MakeCurrent()) {
        driver_version = CollectGLInfo(&gl_version, &shader_version);
      }
      ctx->Destroy();
    }
  }


  // OpenGL doesn't have separate versions for pixel and vertex shader
  // languages, so we just pass the shader_version for both.
  gpu_info->SetGraphicsInfo(vendor_id,
                            device_id,
                            driver_version,
                            shader_version,
                            shader_version,
                            gl_version,
                            false);

  return true;
}

}  // namespace gpu_info_collector
