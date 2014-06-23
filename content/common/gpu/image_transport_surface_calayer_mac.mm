// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface_calayer_mac.h"

#include "base/mac/sdk_forward_declarations.h"
#include "content/common/gpu/surface_handle_types_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/geometry/size_conversions.h"

@interface ImageTransportLayer (Private) {
}
@end

@implementation ImageTransportLayer

- (id)initWithContext:(CGLContextObj)context
          withTexture:(GLuint)texture
        withPixelSize:(gfx::Size)pixelSize
      withScaleFactor:(float)scaleFactor {
  if (self = [super init]) {
    shareContext_.reset(CGLRetainContext(context));
    texture_ = texture;
    pixelSize_ = pixelSize;

    gfx::Size dipSize(gfx::ToFlooredSize(gfx::ScaleSize(
        pixelSize_, 1.0f / scaleFactor)));
    [self setContentsScale:scaleFactor];
    [self setFrame:CGRectMake(0, 0, dipSize.width(), dipSize.height())];
  }
  return self;
}

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask {
  return CGLRetainPixelFormat(CGLGetPixelFormat(shareContext_));
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat {
  CGLContextObj context = NULL;
  CGLError error = CGLCreateContext(pixelFormat, shareContext_, &context);
  if (error != kCGLNoError)
    DLOG(ERROR) << "CGLCreateContext failed with CGL error: " << error;
  return context;
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext
                pixelFormat:(CGLPixelFormatObj)pixelFormat
               forLayerTime:(CFTimeInterval)timeInterval
                displayTime:(const CVTimeStamp*)timeStamp {
  return YES;
}

- (void)drawInCGLContext:(CGLContextObj)glContext
             pixelFormat:(CGLPixelFormatObj)pixelFormat
            forLayerTime:(CFTimeInterval)timeInterval
             displayTime:(const CVTimeStamp*)timeStamp {
  glClearColor(1, 0, 1, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  GLint viewport[4] = {0, 0, 0, 0};
  glGetIntegerv(GL_VIEWPORT, viewport);
  gfx::Size viewportSize(viewport[2], viewport[3]);

  // Set the coordinate system to be one-to-one with pixels.
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, viewportSize.width(), 0, viewportSize.height(), -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Draw a fullscreen quad.
  glColor4f(1, 1, 1, 1);
  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture_);
  glBegin(GL_QUADS);
  {
    glTexCoord2f(0, 0);
    glVertex2f(0, 0);

    glTexCoord2f(0, pixelSize_.height());
    glVertex2f(0, pixelSize_.height());

    glTexCoord2f(pixelSize_.width(), pixelSize_.height());
    glVertex2f(pixelSize_.width(), pixelSize_.height());

    glTexCoord2f(pixelSize_.width(), 0);
    glVertex2f(pixelSize_.width(), 0);
  }
  glEnd();
  glBindTexture(0, texture_);
  glDisable(GL_TEXTURE_RECTANGLE_ARB);

  [super drawInCGLContext:glContext
              pixelFormat:pixelFormat
             forLayerTime:timeInterval
              displayTime:timeStamp];
}

@end

namespace content {

CALayerStorageProvider::CALayerStorageProvider() {
  base::scoped_nsobject<NSDictionary> dict([[NSDictionary alloc] init]);
  CGSConnectionID connection_id = CGSMainConnectionID();
  context_.reset([CAContext contextWithCGSConnection:connection_id
                                             options:dict]);
  [context_ retain];
}

CALayerStorageProvider::~CALayerStorageProvider() {
}

gfx::Size CALayerStorageProvider::GetRoundedSize(gfx::Size size) {
  return size;
}

bool CALayerStorageProvider::AllocateColorBufferStorage(
    CGLContextObj context, GLuint texture,
    gfx::Size pixel_size, float scale_factor) {
  // Allocate an ordinary OpenGL texture to back the FBO.
  GLenum error;
  while ((error = glGetError()) != GL_NO_ERROR) {
    DLOG(ERROR) << "Error found (and ignored) before allocating buffer "
                << "storage: " << error;
  }
  glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,
               0,
               GL_RGBA,
               pixel_size.width(),
               pixel_size.height(),
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               NULL);
  error = glGetError();
  if (error != GL_NO_ERROR) {
    DLOG(ERROR) << "glTexImage failed with GL error: " << error;
    return false;
  }
  glFlush();

  // Disable the fade-in animation as the layer is changed.
  ScopedCAActionDisabler disabler;

  // Resize the CAOpenGLLayer to match the size needed, and change it to be the
  // hosted layer.
  layer_.reset([[ImageTransportLayer alloc] initWithContext:context
                                                withTexture:texture
                                              withPixelSize:pixel_size
                                            withScaleFactor:scale_factor]);
  return true;
}

void CALayerStorageProvider::FreeColorBufferStorage() {
  [context_ setLayer:nil];
  layer_.reset();
}

uint64 CALayerStorageProvider::GetSurfaceHandle() const {
  return SurfaceHandleFromCAContextID([context_ contextId]);
}

void CALayerStorageProvider::WillSwapBuffers() {
  // Don't add the layer to the CAContext until a SwapBuffers is going to be
  // called, because the texture does not have any content until the
  // SwapBuffers call is about to be made.
  if ([context_ layer] != layer_.get())
    [context_ setLayer:layer_];

  // TODO(ccameron): Use the isAsynchronous property to ensure smooth
  // animation.
  [layer_ setNeedsDisplay];
}

}  //  namespace content
