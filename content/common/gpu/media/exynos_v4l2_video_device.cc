// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/debug/trace_event.h"
#include "base/posix/eintr_wrapper.h"
#include "content/common/gpu/media/exynos_v4l2_video_device.h"

namespace content {

namespace {
const char kDevice[] = "/dev/mfc-dec";
}

ExynosV4L2Device::ExynosV4L2Device()
    : device_fd_(-1), device_poll_interrupt_fd_(-1) {}

ExynosV4L2Device::~ExynosV4L2Device() {
  if (device_poll_interrupt_fd_ != -1) {
    close(device_poll_interrupt_fd_);
    device_poll_interrupt_fd_ = -1;
  }
  if (device_fd_ != -1) {
    close(device_fd_);
    device_fd_ = -1;
  }
}

int ExynosV4L2Device::Ioctl(int request, void* arg) {
  return ioctl(device_fd_, request, arg);
}

bool ExynosV4L2Device::Poll(bool poll_device, bool* event_pending) {
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

void* ExynosV4L2Device::Mmap(void* addr,
                             unsigned int len,
                             int prot,
                             int flags,
                             unsigned int offset) {
  return mmap(addr, len, prot, flags, device_fd_, offset);
}

void ExynosV4L2Device::Munmap(void* addr, unsigned int len) {
  munmap(addr, len);
}

bool ExynosV4L2Device::SetDevicePollInterrupt() {
  DVLOG(3) << "SetDevicePollInterrupt()";

  const uint64 buf = 1;
  if (HANDLE_EINTR(write(device_poll_interrupt_fd_, &buf, sizeof(buf))) == -1) {
    return false;
  }
  return true;
}

bool ExynosV4L2Device::ClearDevicePollInterrupt() {
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

bool ExynosV4L2Device::Initialize() {
  DVLOG(2) << "Initialize(): opening device: " << kDevice;
  // Open the video device.
  device_fd_ = HANDLE_EINTR(open(kDevice, O_RDWR | O_NONBLOCK | O_CLOEXEC));
  if (device_fd_ == -1) {
    return false;
  }

  device_poll_interrupt_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (device_poll_interrupt_fd_ == -1) {
    return false;
  }
  return true;
}
}  //  namespace content
