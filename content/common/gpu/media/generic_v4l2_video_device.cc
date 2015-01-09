// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <fcntl.h>
#include <libdrm/drm_fourcc.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/debug/trace_event.h"
#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "content/common/gpu/media/generic_v4l2_video_device.h"
#include "ui/gl/gl_bindings.h"

#ifdef USE_LIBV4L2
// Auto-generated for dlopen libv4l2 libraries
#include "content/common/gpu/media/v4l2_stubs.h"
#include "third_party/v4l-utils/lib/include/libv4l2.h"

using content_common_gpu_media::kModuleV4l2;
using content_common_gpu_media::InitializeStubs;
using content_common_gpu_media::StubPathMap;

static const base::FilePath::CharType kV4l2Lib[] =
    FILE_PATH_LITERAL("libv4l2.so");
#else
#define v4l2_close close
#define v4l2_ioctl ioctl
#endif

namespace content {

namespace {
const char kDecoderDevice[] = "/dev/video-dec";
const char kEncoderDevice[] = "/dev/video-enc";
const char kImageProcessorDevice[] = "/dev/gsc1";
}

GenericV4L2Device::GenericV4L2Device(Type type)
    : type_(type),
      device_fd_(-1),
      device_poll_interrupt_fd_(-1) {}

GenericV4L2Device::~GenericV4L2Device() {
  if (device_poll_interrupt_fd_ != -1) {
    close(device_poll_interrupt_fd_);
    device_poll_interrupt_fd_ = -1;
  }
  if (device_fd_ != -1) {
    v4l2_close(device_fd_);
    device_fd_ = -1;
  }
}

int GenericV4L2Device::Ioctl(int request, void* arg) {
  return HANDLE_EINTR(v4l2_ioctl(device_fd_, request, arg));
}

bool GenericV4L2Device::Poll(bool poll_device, bool* event_pending) {
  struct pollfd pollfds[2];
  nfds_t nfds;
  int pollfd = -1;

  pollfds[0].fd = device_poll_interrupt_fd_;
  pollfds[0].events = POLLIN | POLLERR;
  nfds = 1;

  if (poll_device) {
    DVLOG(3) << "Poll(): adding device fd to poll() set";
    pollfds[nfds].fd = device_fd_;
    pollfds[nfds].events = POLLIN | POLLOUT | POLLERR | POLLPRI;
    pollfd = nfds;
    nfds++;
  }

  if (HANDLE_EINTR(poll(pollfds, nfds, -1)) == -1) {
    DPLOG(ERROR) << "poll() failed";
    return false;
  }
  *event_pending = (pollfd != -1 && pollfds[pollfd].revents & POLLPRI);
  return true;
}

void* GenericV4L2Device::Mmap(void* addr,
                             unsigned int len,
                             int prot,
                             int flags,
                             unsigned int offset) {
  return mmap(addr, len, prot, flags, device_fd_, offset);
}

void GenericV4L2Device::Munmap(void* addr, unsigned int len) {
  munmap(addr, len);
}

bool GenericV4L2Device::SetDevicePollInterrupt() {
  DVLOG(3) << "SetDevicePollInterrupt()";

  const uint64 buf = 1;
  if (HANDLE_EINTR(write(device_poll_interrupt_fd_, &buf, sizeof(buf))) == -1) {
    DPLOG(ERROR) << "SetDevicePollInterrupt(): write() failed";
    return false;
  }
  return true;
}

bool GenericV4L2Device::ClearDevicePollInterrupt() {
  DVLOG(3) << "ClearDevicePollInterrupt()";

  uint64 buf;
  if (HANDLE_EINTR(read(device_poll_interrupt_fd_, &buf, sizeof(buf))) == -1) {
    if (errno == EAGAIN) {
      // No interrupt flag set, and we're reading nonblocking.  Not an error.
      return true;
    } else {
      DPLOG(ERROR) << "ClearDevicePollInterrupt(): read() failed";
      return false;
    }
  }
  return true;
}

bool GenericV4L2Device::Initialize() {
  const char* device_path = NULL;
  static bool v4l2_functions_initialized = PostSandboxInitialization();
  if (!v4l2_functions_initialized) {
    LOG(ERROR) << "Failed to initialize LIBV4L2 libs";
    return false;
  }

  switch (type_) {
    case kDecoder:
      device_path = kDecoderDevice;
      break;
    case kEncoder:
      device_path = kEncoderDevice;
      break;
    case kImageProcessor:
      device_path = kImageProcessorDevice;
      break;
  }

  DVLOG(2) << "Initialize(): opening device: " << device_path;
  // Open the video device.
  device_fd_ = HANDLE_EINTR(open(device_path, O_RDWR | O_NONBLOCK | O_CLOEXEC));
  if (device_fd_ == -1) {
    return false;
  }
#ifdef USE_LIBV4L2
  if (HANDLE_EINTR(v4l2_fd_open(device_fd_, V4L2_DISABLE_CONVERSION)) == -1) {
    v4l2_close(device_fd_);
    return false;
  }
#endif

  device_poll_interrupt_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (device_poll_interrupt_fd_ == -1) {
    return false;
  }
  return true;
}

EGLImageKHR GenericV4L2Device::CreateEGLImage(EGLDisplay egl_display,
                                             EGLContext /* egl_context */,
                                             GLuint texture_id,
                                             gfx::Size frame_buffer_size,
                                             unsigned int buffer_index,
                                             size_t planes_count) {
  DVLOG(3) << "CreateEGLImage()";

  scoped_ptr<base::ScopedFD[]> dmabuf_fds(new base::ScopedFD[planes_count]);
  for (size_t i = 0; i < planes_count; ++i) {
    // Export the DMABUF fd so we can export it as a texture.
    struct v4l2_exportbuffer expbuf;
    memset(&expbuf, 0, sizeof(expbuf));
    expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    expbuf.index = buffer_index;
    expbuf.plane = i;
    expbuf.flags = O_CLOEXEC;
    if (Ioctl(VIDIOC_EXPBUF, &expbuf) != 0) {
      return EGL_NO_IMAGE_KHR;
    }
    dmabuf_fds[i].reset(expbuf.fd);
  }
  DCHECK_EQ(planes_count, 2u);
  EGLint attrs[] = {
      EGL_WIDTH,                     0, EGL_HEIGHT,                    0,
      EGL_LINUX_DRM_FOURCC_EXT,      0, EGL_DMA_BUF_PLANE0_FD_EXT,     0,
      EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0, EGL_DMA_BUF_PLANE0_PITCH_EXT,  0,
      EGL_DMA_BUF_PLANE1_FD_EXT,     0, EGL_DMA_BUF_PLANE1_OFFSET_EXT, 0,
      EGL_DMA_BUF_PLANE1_PITCH_EXT,  0, EGL_NONE, };
  attrs[1] = frame_buffer_size.width();
  attrs[3] = frame_buffer_size.height();
  attrs[5] = DRM_FORMAT_NV12;
  attrs[7] = dmabuf_fds[0].get();
  attrs[9] = 0;
  attrs[11] = frame_buffer_size.width();
  attrs[13] = dmabuf_fds[1].get();
  attrs[15] = 0;
  attrs[17] = frame_buffer_size.width();

  EGLImageKHR egl_image = eglCreateImageKHR(
      egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attrs);
  if (egl_image == EGL_NO_IMAGE_KHR) {
    return egl_image;
  }
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, egl_image);

  return egl_image;
}

EGLBoolean GenericV4L2Device::DestroyEGLImage(EGLDisplay egl_display,
                                             EGLImageKHR egl_image) {
  return eglDestroyImageKHR(egl_display, egl_image);
}

GLenum GenericV4L2Device::GetTextureTarget() { return GL_TEXTURE_EXTERNAL_OES; }

uint32 GenericV4L2Device::PreferredInputFormat() {
  // TODO(posciak): We should support "dontcare" returns here once we
  // implement proper handling (fallback, negotiation) for this in users.
  CHECK_EQ(type_, kEncoder);
  return V4L2_PIX_FMT_NV12M;
}

uint32 GenericV4L2Device::PreferredOutputFormat() {
  // TODO(posciak): We should support "dontcare" returns here once we
  // implement proper handling (fallback, negotiation) for this in users.
  CHECK_EQ(type_, kDecoder);
  return V4L2_PIX_FMT_NV12M;
}

// static
bool GenericV4L2Device::PostSandboxInitialization() {
#ifdef USE_LIBV4L2
  StubPathMap paths;
  paths[kModuleV4l2].push_back(kV4l2Lib);

  return InitializeStubs(paths);
#else
  return true;
#endif
}

}  //  namespace content
