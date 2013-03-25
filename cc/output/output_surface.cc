// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface.h"

#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

using std::set;
using std::string;
using std::vector;

namespace cc {

OutputSurface::OutputSurface(
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d)
    : client_(NULL),
      context3d_(context3d.Pass()),
      has_gl_discard_backbuffer_(false) {
}

OutputSurface::OutputSurface(
    scoped_ptr<cc::SoftwareOutputDevice> software_device)
    : client_(NULL),
      software_device_(software_device.Pass()),
      has_gl_discard_backbuffer_(false) {
}

OutputSurface::OutputSurface(
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d,
    scoped_ptr<cc::SoftwareOutputDevice> software_device)
    : client_(NULL),
      context3d_(context3d.Pass()),
      software_device_(software_device.Pass()),
      has_gl_discard_backbuffer_(false) {
}

OutputSurface::~OutputSurface() {
}

bool OutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  DCHECK(client);
  client_ = client;
  if (!context3d_)
    return true;
  if (!context3d_->makeContextCurrent())
    return false;

  string extensions_string = UTF16ToASCII(context3d_->getString(GL_EXTENSIONS));
  vector<string> extensions_list;
  base::SplitString(extensions_string, ' ', &extensions_list);
  set<string> extensions(extensions_list.begin(), extensions_list.end());

  has_gl_discard_backbuffer_ =
      extensions.count("GL_CHROMIUM_discard_backbuffer");

  return true;
}

void OutputSurface::SendFrameToParentCompositor(CompositorFrame*) {
  NOTIMPLEMENTED();
}

void OutputSurface::EnsureBackbuffer() {
  DCHECK(context3d_);
  if (has_gl_discard_backbuffer_)
    context3d_->ensureBackbufferCHROMIUM();
}

void OutputSurface::DiscardBackbuffer() {
  DCHECK(context3d_);
  if (has_gl_discard_backbuffer_)
    context3d_->discardBackbufferCHROMIUM();
}

void OutputSurface::Reshape(gfx::Size size) {
  DCHECK(context3d_);
  context3d_->reshape(size.width(), size.height());
}

void OutputSurface::BindFramebuffer() {
  DCHECK(context3d_);
  context3d_->bindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OutputSurface::SwapBuffers() {
  DCHECK(context3d_);
  // Note that currently this has the same effect as SwapBuffers; we should
  // consider exposing a different entry point on WebGraphicsContext3D.
  context3d_->prepareTexture();
}

void OutputSurface::PostSubBuffer(gfx::Rect rect) {
  DCHECK(context3d_);
  context3d_->postSubBufferCHROMIUM(
      rect.x(), rect.y(), rect.width(), rect.height());
}

}  // namespace cc
