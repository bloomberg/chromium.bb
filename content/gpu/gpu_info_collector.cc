// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_info_collector.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/strings/string_split.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace {

scoped_refptr<gfx::GLSurface> InitializeGLSurface() {
  scoped_refptr<gfx::GLSurface> surface(
      gfx::GLSurface::CreateOffscreenGLSurface(false, gfx::Size(1, 1)));
  if (!surface.get()) {
    LOG(ERROR) << "gfx::GLContext::CreateOffscreenGLSurface failed";
    return NULL;
  }

  return surface;
}

scoped_refptr<gfx::GLContext> InitializeGLContext(gfx::GLSurface* surface) {

  scoped_refptr<gfx::GLContext> context(
      gfx::GLContext::CreateGLContext(NULL,
                                      surface,
                                      gfx::PreferIntegratedGpu));
  if (!context.get()) {
    LOG(ERROR) << "gfx::GLContext::CreateGLContext failed";
    return NULL;
  }

  if (!context->MakeCurrent(surface)) {
    LOG(ERROR) << "gfx::GLContext::MakeCurrent() failed";
    return NULL;
  }

  return context;
}

std::string GetGLString(unsigned int pname) {
  const char* gl_string =
      reinterpret_cast<const char*>(glGetString(pname));
  if (gl_string)
    return std::string(gl_string);
  return "";
}

// Return a version string in the format of "major.minor".
std::string GetVersionFromString(const std::string& version_string) {
  size_t begin = version_string.find_first_of("0123456789");
  if (begin != std::string::npos) {
    size_t end = version_string.find_first_not_of("01234567890.", begin);
    std::string sub_string;
    if (end != std::string::npos)
      sub_string = version_string.substr(begin, end - begin);
    else
      sub_string = version_string.substr(begin);
    std::vector<std::string> pieces;
    base::SplitString(sub_string, '.', &pieces);
    if (pieces.size() >= 2)
      return pieces[0] + "." + pieces[1];
  }
  return "";
}

}  // namespace anonymous

namespace gpu_info_collector {

bool CollectGraphicsInfoGL(content::GPUInfo* gpu_info) {
  if (!gfx::GLSurface::InitializeOneOff()) {
    LOG(ERROR) << "gfx::GLSurface::InitializeOneOff() failed";
    return false;
  }

  scoped_refptr<gfx::GLSurface> surface(InitializeGLSurface());
  if (!surface.get())
    return false;

  scoped_refptr<gfx::GLContext> context(InitializeGLContext(surface.get()));
  if (!context.get())
    return false;

  gpu_info->gl_renderer = GetGLString(GL_RENDERER);
  gpu_info->gl_vendor = GetGLString(GL_VENDOR);
  gpu_info->gl_extensions = GetGLString(GL_EXTENSIONS);
  gpu_info->gl_version_string = GetGLString(GL_VERSION);
  std::string glsl_version_string = GetGLString(GL_SHADING_LANGUAGE_VERSION);
  // TODO(kbr): remove once the destruction of a current context automatically
  // clears the current context.
  context->ReleaseCurrent(surface.get());

  gpu_info->gl_version = GetVersionFromString(gpu_info->gl_version_string);
  std::string glsl_version = GetVersionFromString(glsl_version_string);
  gpu_info->pixel_shader_version = glsl_version;
  gpu_info->vertex_shader_version = glsl_version;

  return CollectDriverInfoGL(gpu_info);
}

void MergeGPUInfoGL(content::GPUInfo* basic_gpu_info,
                    const content::GPUInfo& context_gpu_info) {
  DCHECK(basic_gpu_info);
  basic_gpu_info->gl_renderer = context_gpu_info.gl_renderer;
  basic_gpu_info->gl_vendor = context_gpu_info.gl_vendor;
  basic_gpu_info->gl_version_string = context_gpu_info.gl_version_string;
  basic_gpu_info->gl_extensions = context_gpu_info.gl_extensions;
  basic_gpu_info->gl_version = context_gpu_info.gl_version;
  basic_gpu_info->pixel_shader_version =
      context_gpu_info.pixel_shader_version;
  basic_gpu_info->vertex_shader_version =
      context_gpu_info.vertex_shader_version;

  if (!context_gpu_info.driver_vendor.empty())
    basic_gpu_info->driver_vendor = context_gpu_info.driver_vendor;
  if (!context_gpu_info.driver_version.empty())
    basic_gpu_info->driver_version = context_gpu_info.driver_version;

  basic_gpu_info->can_lose_context = context_gpu_info.can_lose_context;
  basic_gpu_info->sandboxed = context_gpu_info.sandboxed;
  basic_gpu_info->gpu_accessible = context_gpu_info.gpu_accessible;
  basic_gpu_info->finalized = context_gpu_info.finalized;
  basic_gpu_info->initialization_time = context_gpu_info.initialization_time;
}

}  // namespace gpu_info_collector

