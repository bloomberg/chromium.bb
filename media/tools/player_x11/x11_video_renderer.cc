// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/player_x11/x11_video_renderer.h"

#include <dlfcn.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>

#include "media/base/buffers.h"
#include "media/base/yuv_convert.h"

X11VideoRenderer* X11VideoRenderer::instance_ = NULL;

// Returns the picture format for ARGB.
// This method is originally from chrome/common/x11_util.cc.
static XRenderPictFormat* GetRenderARGB32Format(Display* dpy) {
  static XRenderPictFormat* pictformat = NULL;
  if (pictformat)
    return pictformat;

  // First look for a 32-bit format which ignores the alpha value.
  XRenderPictFormat templ;
  templ.depth = 32;
  templ.type = PictTypeDirect;
  templ.direct.red = 16;
  templ.direct.green = 8;
  templ.direct.blue = 0;
  templ.direct.redMask = 0xff;
  templ.direct.greenMask = 0xff;
  templ.direct.blueMask = 0xff;
  templ.direct.alphaMask = 0;

  static const unsigned long kMask =
      PictFormatType | PictFormatDepth |
      PictFormatRed | PictFormatRedMask |
      PictFormatGreen | PictFormatGreenMask |
      PictFormatBlue | PictFormatBlueMask |
      PictFormatAlphaMask;

  pictformat = XRenderFindFormat(dpy, kMask, &templ, 0 /* first result */);

  if (!pictformat) {
    // Not all X servers support xRGB32 formats. However, the XRENDER spec
    // says that they must support an ARGB32 format, so we can always return
    // that.
    pictformat = XRenderFindStandardFormat(dpy, PictStandardARGB32);
    CHECK(pictformat) << "XRENDER ARGB32 not supported.";
  }

  return pictformat;
}

X11VideoRenderer::X11VideoRenderer(Display* display, Window window)
    : display_(display),
      window_(window),
      image_(NULL),
      new_frame_(false),
      picture_(0),
      use_render_(false),
      use_gl_(false),
      gl_context_(NULL) {
  // Save the instance of the video renderer.
  CHECK(!instance_);
  instance_ = this;
}

X11VideoRenderer::~X11VideoRenderer() {
  CHECK(instance_);
  instance_ = NULL;
}

// static
bool X11VideoRenderer::IsMediaFormatSupported(
    const media::MediaFormat& media_format) {
  int width = 0;
  int height = 0;
  return ParseMediaFormat(media_format, &width, &height);
}

void X11VideoRenderer::OnStop() {
  if (use_gl_) {
    glXMakeCurrent(display_, 0, NULL);
    glXDestroyContext(display_, gl_context_);
  }
  if (image_) {
    XDestroyImage(image_);
  }
  if (use_render_) {
    XRenderFreePicture(display_, picture_);
  }
}

static GLXContext InitGLContext(Display* display, Window window) {
  // Some versions of NVIDIA's GL libGL.so include a broken version of
  // dlopen/dlsym, and so linking it into chrome breaks it. So we dynamically
  // load it, and use glew to dynamically resolve symbols.
  // See http://code.google.com/p/chromium/issues/detail?id=16800
  void* handle = dlopen("libGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
  if (!handle) {
    LOG(ERROR) << "Could not find libGL.so.1";
    return NULL;
  }
  if (glxewInit() != GLEW_OK) {
    LOG(ERROR) << "GLXEW failed initialization";
    return NULL;
  }

  XWindowAttributes attributes;
  XGetWindowAttributes(display, window, &attributes);
  XVisualInfo visual_info_template;
  visual_info_template.visualid = XVisualIDFromVisual(attributes.visual);
  int visual_info_count = 0;
  XVisualInfo* visual_info_list = XGetVisualInfo(display, VisualIDMask,
                                                 &visual_info_template,
                                                 &visual_info_count);
  GLXContext context = NULL;
  for (int i = 0; i < visual_info_count && !context; ++i) {
    context = glXCreateContext(display, visual_info_list + i, 0,
                               True /* Direct rendering */);
  }

  XFree(visual_info_list);
  if (!context) {
    return NULL;
  }

  if (!glXMakeCurrent(display, window, context)) {
    glXDestroyContext(display, context);
    return NULL;
  }

  if (glewInit() != GLEW_OK) {
    LOG(ERROR) << "GLEW failed initialization";
    glXDestroyContext(display, context);
    return NULL;
  }

  if (!glewIsSupported("GL_VERSION_2_0")) {
    LOG(ERROR) << "GL implementation doesn't support GL version 2.0";
    glXDestroyContext(display, context);
    return NULL;
  }

  return context;
}

// Matrix used for the YUV to RGB conversion.
static const float kYUV2RGB[9] = {
  1.f, 0.f, 1.403f,
  1.f, -.344f, -.714f,
  1.f, 1.772f, 0.f,
};

// Vertices for a full screen quad.
static const float kVertices[8] = {
  -1.f, 1.f,
  -1.f, -1.f,
  1.f, 1.f,
  1.f, -1.f,
};

// Texture Coordinates mapping the entire texture.
static const float kTextureCoords[8] = {
  0, 0,
  0, 1,
  1, 0,
  1, 1,
};

// Pass-through vertex shader.
static const char kVertexShader[] =
    "varying vec2 interp_tc;\n"
    "\n"
    "attribute vec4 in_pos;\n"
    "attribute vec2 in_tc;\n"
    "\n"
    "void main() {\n"
    "  interp_tc = in_tc;\n"
    "  gl_Position = in_pos;\n"
    "}\n";

// YUV to RGB pixel shader. Loads a pixel from each plane and pass through the
// matrix.
static const char kFragmentShader[] =
    "varying vec2 interp_tc;\n"
    "\n"
    "uniform sampler2D y_tex;\n"
    "uniform sampler2D u_tex;\n"
    "uniform sampler2D v_tex;\n"
    "uniform mat3 yuv2rgb;\n"
    "\n"
    "void main() {\n"
    "  float y = texture2D(y_tex, interp_tc).x;\n"
    "  float u = texture2D(u_tex, interp_tc).r - .5;\n"
    "  float v = texture2D(v_tex, interp_tc).r - .5;\n"
    "  vec3 rgb = yuv2rgb * vec3(y, u, v);\n"
    "  gl_FragColor = vec4(rgb, 1);\n"
    "}\n";

// Buffer size for compile errors.
static const unsigned int kErrorSize = 4096;

bool X11VideoRenderer::OnInitialize(media::VideoDecoder* decoder) {
  if (!ParseMediaFormat(decoder->media_format(), &width_, &height_))
    return false;

  // Resize the window to fit that of the video.
  XResizeWindow(display_, window_, width_, height_);

  gl_context_ = InitGLContext(display_, window_);
  use_gl_ = (gl_context_ != NULL);

  if (use_gl_) {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glViewport(0, 0, width_, height_);

    // Create 3 textures, one for each plane, and bind them to different
    // texture units.
    glGenTextures(media::VideoSurface::kNumYUVPlanes, textures_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures_[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures_[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, textures_[2]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);

    GLuint program_ = glCreateProgram();

    // Create our YUV->RGB shader.
    GLuint vertex_shader_ = glCreateShader(GL_VERTEX_SHADER);
    const char* vs_source = kVertexShader;
    int vs_size = sizeof(kVertexShader);
    glShaderSource(vertex_shader_, 1, &vs_source, &vs_size);
    glCompileShader(vertex_shader_);
    int result = GL_FALSE;
    glGetShaderiv(vertex_shader_, GL_COMPILE_STATUS, &result);
    if (!result) {
      char log[kErrorSize];
      int len;
      glGetShaderInfoLog(vertex_shader_, kErrorSize - 1, &len, log);
      log[kErrorSize - 1] = 0;
      LOG(FATAL) << log;
    }
    glAttachShader(program_, vertex_shader_);

    GLuint fragment_shader_ = glCreateShader(GL_FRAGMENT_SHADER);
    const char* ps_source = kFragmentShader;
    int ps_size = sizeof(kFragmentShader);
    glShaderSource(fragment_shader_, 1, &ps_source, &ps_size);
    glCompileShader(fragment_shader_);
    result = GL_FALSE;
    glGetShaderiv(fragment_shader_, GL_COMPILE_STATUS, &result);
    if (!result) {
      char log[kErrorSize];
      int len;
      glGetShaderInfoLog(fragment_shader_, kErrorSize - 1, &len, log);
      log[kErrorSize - 1] = 0;
      LOG(FATAL) << log;
    }
    glAttachShader(program_, fragment_shader_);

    glLinkProgram(program_);
    result = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &result);
    if (!result) {
      char log[kErrorSize];
      int len;
      glGetProgramInfoLog(program_, kErrorSize - 1, &len, log);
      log[kErrorSize - 1] = 0;
      LOG(FATAL) << log;
    }
    glUseProgram(program_);

    // Bind parameters.
    glUniform1i(glGetUniformLocation(program_, "y_tex"), 0);
    glUniform1i(glGetUniformLocation(program_, "u_tex"), 1);
    glUniform1i(glGetUniformLocation(program_, "v_tex"), 2);
    int yuv2rgb_location = glGetUniformLocation(program_, "yuv2rgb");
    glUniformMatrix3fv(yuv2rgb_location, 1, GL_TRUE, kYUV2RGB);

    int pos_location = glGetAttribLocation(program_, "in_pos");
    glEnableVertexAttribArray(pos_location);
    glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, kVertices);

    int tc_location = glGetAttribLocation(program_, "in_tc");
    glEnableVertexAttribArray(tc_location);
    glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0,
                          kTextureCoords);

    // We are getting called on a thread. Release the context so that it can be
    // made current on the main thread.
    glXMakeCurrent(display_, 0, NULL);
  } else {
    // Testing XRender support. We'll use the very basic of XRender
    // so if it presents it is already good enough. We don't need
    // to check its version.
    int dummy;
    use_render_ = XRenderQueryExtension(display_, &dummy, &dummy);

    if (use_render_) {
      // If we are using XRender, we'll create a picture representing the
      // window.
      XWindowAttributes attr;
      XGetWindowAttributes(display_, window_, &attr);

      XRenderPictFormat* pictformat = XRenderFindVisualFormat(
          display_,
          attr.visual);
      CHECK(pictformat) << "XRENDER does not support default visual";

      picture_ = XRenderCreatePicture(display_, window_, pictformat, 0, NULL);
      CHECK(picture_) << "Backing picture not created";
    }

    // Initialize the XImage to store the output of YUV -> RGB conversion.
    image_ = XCreateImage(display_,
                          DefaultVisual(display_, DefaultScreen(display_)),
                          DefaultDepth(display_, DefaultScreen(display_)),
                          ZPixmap,
                          0,
                          static_cast<char*>(malloc(width_ * height_ * 4)),
                          width_,
                          height_,
                          32,
                          width_ * 4);
    DCHECK(image_);
  }
  return true;
}

void X11VideoRenderer::OnFrameAvailable() {
  AutoLock auto_lock(lock_);
  new_frame_ = true;
}

void X11VideoRenderer::Paint() {
  // Use |new_frame_| to prevent overdraw since Paint() is called more
  // often than needed. It is OK to lock only this flag and we don't
  // want to lock the whole function because this method takes a long
  // time to complete.
  {
    AutoLock auto_lock(lock_);
    if (!new_frame_)
      return;
    new_frame_ = false;
  }

  scoped_refptr<media::VideoFrame> video_frame;
  GetCurrentFrame(&video_frame);

  if ((!use_gl_ && !image_) || !video_frame)
    return;

  // Convert YUV frame to RGB.
  media::VideoSurface frame_in;
  if (video_frame->Lock(&frame_in)) {
    DCHECK(frame_in.format == media::VideoSurface::YV12 ||
           frame_in.format == media::VideoSurface::YV16);
    DCHECK(frame_in.strides[media::VideoSurface::kUPlane] ==
           frame_in.strides[media::VideoSurface::kVPlane]);
    DCHECK(frame_in.planes == media::VideoSurface::kNumYUVPlanes);

    if (use_gl_) {
      if (glXGetCurrentContext() != gl_context_ ||
          glXGetCurrentDrawable() != window_) {
        glXMakeCurrent(display_, window_, gl_context_);
      }
      for (unsigned int i = 0; i < media::VideoSurface::kNumYUVPlanes; ++i) {
        unsigned int width = (i == media::VideoSurface::kYPlane) ?
            frame_in.width : frame_in.width / 2;
        unsigned int height = (i == media::VideoSurface::kYPlane ||
                               frame_in.format == media::VideoSurface::YV16) ?
            frame_in.height : frame_in.height / 2;
        glActiveTexture(GL_TEXTURE0 + i);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame_in.strides[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, frame_in.data[i]);
      }
    } else {
      DCHECK(image_->data);
      media::YUVType yuv_type = (frame_in.format == media::VideoSurface::YV12) ?
          media::YV12 : media::YV16;
      media::ConvertYUVToRGB32(frame_in.data[media::VideoSurface::kYPlane],
                               frame_in.data[media::VideoSurface::kUPlane],
                               frame_in.data[media::VideoSurface::kVPlane],
                               (uint8*)image_->data,
                               frame_in.width,
                               frame_in.height,
                               frame_in.strides[media::VideoSurface::kYPlane],
                               frame_in.strides[media::VideoSurface::kUPlane],
                               image_->bytes_per_line,
                               yuv_type);
    }
    video_frame->Unlock();
  } else {
    NOTREACHED();
  }

  if (use_gl_) {
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glXSwapBuffers(display_, window_);
    return;
  }

  if (use_render_) {
    // If XRender is used, we'll upload the image to a pixmap. And then
    // creats a picture from the pixmap and composite the picture over
    // the picture represending the window.

    // Creates a XImage.
    XImage image;
    memset(&image, 0, sizeof(image));
    image.width = width_;
    image.height = height_;
    image.depth = 32;
    image.bits_per_pixel = 32;
    image.format = ZPixmap;
    image.byte_order = LSBFirst;
    image.bitmap_unit = 8;
    image.bitmap_bit_order = LSBFirst;
    image.bytes_per_line = image_->bytes_per_line;
    image.red_mask = 0xff;
    image.green_mask = 0xff00;
    image.blue_mask = 0xff0000;
    image.data = image_->data;

    // Creates a pixmap and uploads from the XImage.
    unsigned long pixmap = XCreatePixmap(display_,
                                         window_,
                                         width_,
                                         height_,
                                         32);
    GC gc = XCreateGC(display_, pixmap, 0, NULL);
    XPutImage(display_, pixmap, gc, &image,
              0, 0, 0, 0,
              width_, height_);
    XFreeGC(display_, gc);

    // Creates the picture representing the pixmap.
    unsigned long picture = XRenderCreatePicture(
        display_, pixmap, GetRenderARGB32Format(display_), 0, NULL);

    // Composite the picture over the picture representing the window.
    XRenderComposite(display_, PictOpSrc, picture, 0,
                     picture_, 0, 0, 0, 0, 0, 0,
                     width_, height_);

    XRenderFreePicture(display_, picture);
    XFreePixmap(display_, pixmap);
    return;
  }

  // If XRender is not used, simply put the image to the server.
  // This will have a tearing effect but this is OK.
  // TODO(hclam): Upload the image to a pixmap and do XCopyArea()
  // to the window.
  GC gc = XCreateGC(display_, window_, 0, NULL);
  XPutImage(display_, window_, gc, image_,
            0, 0, 0, 0, width_, height_);
  XFlush(display_);
  XFreeGC(display_, gc);
}
