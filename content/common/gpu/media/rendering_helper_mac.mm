// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/rendering_helper.h"

#import <Cocoa/Cocoa.h>
#import <OpenGL/CGLMacro.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/memory/scoped_nsobject.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"

// Gets the pixel format to be used by the OpenGL view.
static scoped_nsobject<NSOpenGLPixelFormat> GetPixelFormat() {
  NSOpenGLPixelFormatAttribute attributes[] = {
    NSOpenGLPFAWindow,
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAAccelerated,
    NSOpenGLPFANoRecovery,
    NSOpenGLPFAColorSize, (NSOpenGLPixelFormatAttribute)32,
    NSOpenGLPFAAlphaSize, (NSOpenGLPixelFormatAttribute)8,
    NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)24,
    (NSOpenGLPixelFormatAttribute)0
  };
  return scoped_nsobject<NSOpenGLPixelFormat>(
    [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes]);
}

// Gets the CGLContext from the given OpenGL view.
static CGLContextObj GetCGLContext(NSOpenGLView* gl_view) {
  return static_cast<CGLContextObj>([[gl_view openGLContext] CGLContextObj]);
}

// Sets up a view port for the OpenGL view.
static void SetupGLViewPort(NSOpenGLView* gl_view, int width, int height) {
  CGLContextObj cgl_ctx = GetCGLContext(gl_view);
  glViewport(0, 0, width, height);
  glClearColor(1.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  [[gl_view openGLContext] flushBuffer];
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

// Draw the given texture to the OpenGL view.
static void DrawTexture(NSOpenGLView* gl_view,
                        GLuint texture_id,
                        bool suppress_swap_to_display) {
  CGLContextObj cgl_ctx = GetCGLContext(gl_view);
  [gl_view lockFocus];

  GLfloat width = [gl_view bounds].size.width;
  GLfloat height = [gl_view bounds].size.height;

  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture_id);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  glBegin(GL_QUADS);
    glTexCoord2f(0.0, height);
    glVertex3f(-1.0, -1.0, 0.0);
    glTexCoord2f(width, height);
    glVertex3f(1.0, -1.0, 0.0);
    glTexCoord2f(width, 0.0);
    glVertex3f(1.0, 1.0, 0.0);
    glTexCoord2f(0.0, 0.0);
    glVertex3f(-1.0, 1.0, 0.0);
  glEnd();

  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
  glDisable(GL_TEXTURE_RECTANGLE_ARB);

  if (!suppress_swap_to_display)
    [[gl_view openGLContext] flushBuffer];
  [gl_view unlockFocus];
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

namespace content {

class RenderingHelperMac : public RenderingHelper {
 public:
  RenderingHelperMac();
  virtual ~RenderingHelperMac();

  // Implement RenderingHelper.
  virtual void Initialize(bool suppress_swap_to_display,
                          int num_windows,
                          int width,
                          int height,
                          base::WaitableEvent* done) OVERRIDE;
  virtual void UnInitialize(base::WaitableEvent* done) OVERRIDE;
  virtual void CreateTexture(int window_id,
                             uint32 texture_target,
                             GLuint* texture_id,
                             base::WaitableEvent* done) OVERRIDE;
  virtual void RenderTexture(GLuint texture_id) OVERRIDE;
  virtual void DeleteTexture(GLuint texture_id) OVERRIDE;
  virtual void* GetGLContext() OVERRIDE;
  virtual void* GetGLDisplay() OVERRIDE;

 private:
  MessageLoop* message_loop_;
  int width_;
  int height_;
  bool suppress_swap_to_display_;
  scoped_nsobject<NSWindow> window_;
  scoped_nsobject<NSOpenGLView> gl_view_;
  base::mac::ScopedNSAutoreleasePool pool_;
};

// static
RenderingHelper* RenderingHelper::Create() {
  return new RenderingHelperMac;
}

// static
void RenderingHelper::InitializePlatform() {
  // Initialize the Cocoa framework.
  base::mac::ScopedNSAutoreleasePool pool_;
  [NSApplication sharedApplication];
}

RenderingHelperMac::RenderingHelperMac()
    : message_loop_(NULL),
      width_(0),
      height_(0),
      suppress_swap_to_display_(false) {
}

RenderingHelperMac::~RenderingHelperMac() {
  CHECK_EQ(width_, 0) << "Must call UnInitialize before dtor.";
}

void RenderingHelperMac::Initialize(bool suppress_swap_to_display,
                                    int num_windows,
                                    int width,
                                    int height,
                                    base::WaitableEvent* done) {
  // Use width_ != 0 as a proxy for the class having already been
  // Initialize()'d, and UnInitialize() before continuing.
  if (width_) {
    base::WaitableEvent done2(false, false);
    UnInitialize(&done2);
    done2.Wait();
  }

  // A separate window is created for each decoder. Since the Mac API
  // only supports a single instance only one window should be created.
  CHECK_EQ(num_windows, 1);

  width_ = width;
  height_ = height;
  suppress_swap_to_display_ = suppress_swap_to_display;
  message_loop_ = MessageLoop::current();

  // Create a window to host the OpenGL contents.
  NSRect rect = NSMakeRect(0, 0, width_, height_);
  window_.reset([[NSWindow alloc]
      initWithContentRect:rect
                styleMask:NSTitledWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [window_ center];
  [window_ makeKeyAndOrderFront:nil];

  // Create an OpenGL view.
  scoped_nsobject<NSOpenGLPixelFormat> pixel_format(GetPixelFormat());
  gl_view_.reset([[NSOpenGLView alloc] initWithFrame:rect
                                         pixelFormat:pixel_format]);
  [[window_ contentView] addSubview:gl_view_];
  SetupGLViewPort(gl_view_, width_, height_);

  done->Signal();
}

void RenderingHelperMac::UnInitialize(base::WaitableEvent* done) {
  CHECK_EQ(MessageLoop::current(), message_loop_);
  width_ = 0;
  height_ = 0;
  message_loop_ = NULL;
  [window_ close];
  window_.reset();
  gl_view_.reset();
  done->Signal();
}

void RenderingHelperMac::CreateTexture(int window_id,
                                       uint32 texture_target,
                                       GLuint* texture_id,
                                       base::WaitableEvent* done) {
  CHECK_EQ(MessageLoop::current(), message_loop_);
  CHECK_EQ(static_cast<uint32>(GL_TEXTURE_RECTANGLE_ARB), texture_target);
  CGLContextObj cgl_ctx = GetCGLContext(gl_view_);
  glGenTextures(1, texture_id);
  CHECK_EQ(GL_NO_ERROR, static_cast<int>(glGetError()));
  done->Signal();
}

void RenderingHelperMac::RenderTexture(GLuint texture_id) {
  CHECK_EQ(MessageLoop::current(), message_loop_);
  DrawTexture(gl_view_, texture_id, suppress_swap_to_display_);
}

void RenderingHelperMac::DeleteTexture(GLuint texture_id) {
  CHECK_EQ(MessageLoop::current(), message_loop_);
  CGLContextObj cgl_ctx = GetCGLContext(gl_view_);
  glDeleteTextures(1, &texture_id);
  CHECK_EQ(GL_NO_ERROR, static_cast<int>(glGetError()));
}

void* RenderingHelperMac::GetGLContext() {
  return GetCGLContext(gl_view_);
}

void* RenderingHelperMac::GetGLDisplay() {
  return NULL;
}

}  // namespace content
