// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/sandbox_ipc_linux.h"

#include <fcntl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/linux_util.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/shared_memory.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/font_utils_linux.h"
#include "content/common/font_config_ipc_linux.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "content/common/set_process_title.h"
#include "content/public/common/content_switches.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "third_party/WebKit/public/platform/linux/WebFontInfo.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/npapi/bindings/npapi_extensions.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"
#include "ui/gfx/font_render_params.h"

using blink::WebCString;
using blink::WebFontInfo;
using blink::WebUChar;
using blink::WebUChar32;

namespace content {

SandboxIPCHandler::SandboxIPCHandler(int lifeline_fd, int browser_socket)
    : lifeline_fd_(lifeline_fd), browser_socket_(browser_socket) {
  // FontConfig doesn't provide a standard property to control subpixel
  // positioning, so we pass the current setting through to WebKit.
  WebFontInfo::setSubpixelPositioning(
      gfx::GetDefaultWebKitFontRenderParams().subpixel_positioning);
}

void SandboxIPCHandler::Run() {
  struct pollfd pfds[2];
  pfds[0].fd = lifeline_fd_;
  pfds[0].events = POLLIN;
  pfds[1].fd = browser_socket_;
  pfds[1].events = POLLIN;

  int failed_polls = 0;
  for (;;) {
    const int r =
        HANDLE_EINTR(poll(pfds, arraysize(pfds), -1 /* no timeout */));
    // '0' is not a possible return value with no timeout.
    DCHECK_NE(0, r);
    if (r < 0) {
      PLOG(WARNING) << "poll";
      if (failed_polls++ == 3) {
        LOG(FATAL) << "poll(2) failing. SandboxIPCHandler aborting.";
        return;
      }
      continue;
    }

    failed_polls = 0;

    // The browser process will close the other end of this pipe on shutdown,
    // so we should exit.
    if (pfds[0].revents) {
      break;
    }

    // If poll(2) reports an error condition in this fd,
    // we assume the zygote is gone and we exit the loop.
    if (pfds[1].revents & (POLLERR | POLLHUP)) {
      break;
    }

    if (pfds[1].revents & POLLIN) {
      HandleRequestFromRenderer(browser_socket_);
    }
  }

  VLOG(1) << "SandboxIPCHandler stopping.";
}

void SandboxIPCHandler::HandleRequestFromRenderer(int fd) {
  ScopedVector<base::ScopedFD> fds;

  // A FontConfigIPC::METHOD_MATCH message could be kMaxFontFamilyLength
  // bytes long (this is the largest message type).
  // 128 bytes padding are necessary so recvmsg() does not return MSG_TRUNC
  // error for a maximum length message.
  char buf[FontConfigIPC::kMaxFontFamilyLength + 128];

  const ssize_t len = UnixDomainSocket::RecvMsg(fd, buf, sizeof(buf), &fds);
  if (len == -1) {
    // TODO: should send an error reply, or the sender might block forever.
    NOTREACHED() << "Sandbox host message is larger than kMaxFontFamilyLength";
    return;
  }
  if (fds.empty())
    return;

  Pickle pickle(buf, len);
  PickleIterator iter(pickle);

  int kind;
  if (!pickle.ReadInt(&iter, &kind))
    return;

  if (kind == FontConfigIPC::METHOD_MATCH) {
    HandleFontMatchRequest(fd, pickle, iter, fds.get());
  } else if (kind == FontConfigIPC::METHOD_OPEN) {
    HandleFontOpenRequest(fd, pickle, iter, fds.get());
  } else if (kind == LinuxSandbox::METHOD_GET_FALLBACK_FONT_FOR_CHAR) {
    HandleGetFallbackFontForChar(fd, pickle, iter, fds.get());
  } else if (kind == LinuxSandbox::METHOD_LOCALTIME) {
    HandleLocaltime(fd, pickle, iter, fds.get());
  } else if (kind == LinuxSandbox::METHOD_GET_STYLE_FOR_STRIKE) {
    HandleGetStyleForStrike(fd, pickle, iter, fds.get());
  } else if (kind == LinuxSandbox::METHOD_MAKE_SHARED_MEMORY_SEGMENT) {
    HandleMakeSharedMemorySegment(fd, pickle, iter, fds.get());
  } else if (kind == LinuxSandbox::METHOD_MATCH_WITH_FALLBACK) {
    HandleMatchWithFallback(fd, pickle, iter, fds.get());
  }
}

int SandboxIPCHandler::FindOrAddPath(const SkString& path) {
  int count = paths_.count();
  for (int i = 0; i < count; ++i) {
    if (path == *paths_[i])
      return i;
  }
  *paths_.append() = new SkString(path);
  return count;
}

void SandboxIPCHandler::HandleFontMatchRequest(
    int fd,
    const Pickle& pickle,
    PickleIterator iter,
    const std::vector<base::ScopedFD*>& fds) {
  uint32_t requested_style;
  std::string family;
  if (!pickle.ReadString(&iter, &family) ||
      !pickle.ReadUInt32(&iter, &requested_style))
    return;

  SkFontConfigInterface::FontIdentity result_identity;
  SkString result_family;
  SkTypeface::Style result_style;
  SkFontConfigInterface* fc =
      SkFontConfigInterface::GetSingletonDirectInterface();
  const bool r =
      fc->matchFamilyName(family.c_str(),
                          static_cast<SkTypeface::Style>(requested_style),
                          &result_identity,
                          &result_family,
                          &result_style);

  Pickle reply;
  if (!r) {
    reply.WriteBool(false);
  } else {
    // Stash away the returned path, so we can give it an ID (index)
    // which will later be given to us in a request to open the file.
    int index = FindOrAddPath(result_identity.fString);
    result_identity.fID = static_cast<uint32_t>(index);

    reply.WriteBool(true);
    skia::WriteSkString(&reply, result_family);
    skia::WriteSkFontIdentity(&reply, result_identity);
    reply.WriteUInt32(result_style);
  }
  SendRendererReply(fds, reply, -1);
}

void SandboxIPCHandler::HandleFontOpenRequest(
    int fd,
    const Pickle& pickle,
    PickleIterator iter,
    const std::vector<base::ScopedFD*>& fds) {
  uint32_t index;
  if (!pickle.ReadUInt32(&iter, &index))
    return;
  if (index >= static_cast<uint32_t>(paths_.count()))
    return;
  const int result_fd = open(paths_[index]->c_str(), O_RDONLY);

  Pickle reply;
  if (result_fd == -1) {
    reply.WriteBool(false);
  } else {
    reply.WriteBool(true);
  }

  // The receiver will have its own access to the file, so we will close it
  // after this send.
  SendRendererReply(fds, reply, result_fd);

  if (result_fd >= 0) {
    int err = IGNORE_EINTR(close(result_fd));
    DCHECK(!err);
  }
}

void SandboxIPCHandler::HandleGetFallbackFontForChar(
    int fd,
    const Pickle& pickle,
    PickleIterator iter,
    const std::vector<base::ScopedFD*>& fds) {
  // The other side of this call is
  // content/common/child_process_sandbox_support_impl_linux.cc

  EnsureWebKitInitialized();
  WebUChar32 c;
  if (!pickle.ReadInt(&iter, &c))
    return;

  std::string preferred_locale;
  if (!pickle.ReadString(&iter, &preferred_locale))
    return;

  blink::WebFallbackFont fallbackFont;
  WebFontInfo::fallbackFontForChar(c, preferred_locale.c_str(), &fallbackFont);

  int pathIndex = FindOrAddPath(SkString(fallbackFont.filename.data()));
  fallbackFont.fontconfigInterfaceId = pathIndex;

  Pickle reply;
  if (fallbackFont.name.data()) {
    reply.WriteString(fallbackFont.name.data());
  } else {
    reply.WriteString(std::string());
  }
  if (fallbackFont.filename.data()) {
    reply.WriteString(fallbackFont.filename.data());
  } else {
    reply.WriteString(std::string());
  }
  reply.WriteInt(fallbackFont.fontconfigInterfaceId);
  reply.WriteInt(fallbackFont.ttcIndex);
  reply.WriteBool(fallbackFont.isBold);
  reply.WriteBool(fallbackFont.isItalic);
  SendRendererReply(fds, reply, -1);
}

void SandboxIPCHandler::HandleGetStyleForStrike(
    int fd,
    const Pickle& pickle,
    PickleIterator iter,
    const std::vector<base::ScopedFD*>& fds) {
  std::string family;
  int sizeAndStyle;

  if (!pickle.ReadString(&iter, &family) ||
      !pickle.ReadInt(&iter, &sizeAndStyle)) {
    return;
  }

  EnsureWebKitInitialized();
  blink::WebFontRenderStyle style;
  WebFontInfo::renderStyleForStrike(family.c_str(), sizeAndStyle, &style);

  Pickle reply;
  reply.WriteInt(style.useBitmaps);
  reply.WriteInt(style.useAutoHint);
  reply.WriteInt(style.useHinting);
  reply.WriteInt(style.hintStyle);
  reply.WriteInt(style.useAntiAlias);
  reply.WriteInt(style.useSubpixelRendering);
  reply.WriteInt(style.useSubpixelPositioning);

  SendRendererReply(fds, reply, -1);
}

void SandboxIPCHandler::HandleLocaltime(
    int fd,
    const Pickle& pickle,
    PickleIterator iter,
    const std::vector<base::ScopedFD*>& fds) {
  // The other side of this call is in zygote_main_linux.cc

  std::string time_string;
  if (!pickle.ReadString(&iter, &time_string) ||
      time_string.size() != sizeof(time_t)) {
    return;
  }

  time_t time;
  memcpy(&time, time_string.data(), sizeof(time));
  // We use localtime here because we need the tm_zone field to be filled
  // out. Since we are a single-threaded process, this is safe.
  const struct tm* expanded_time = localtime(&time);

  std::string result_string;
  const char* time_zone_string = "";
  if (expanded_time != NULL) {
    result_string = std::string(reinterpret_cast<const char*>(expanded_time),
                                sizeof(struct tm));
    time_zone_string = expanded_time->tm_zone;
  }

  Pickle reply;
  reply.WriteString(result_string);
  reply.WriteString(time_zone_string);
  SendRendererReply(fds, reply, -1);
}

void SandboxIPCHandler::HandleMakeSharedMemorySegment(
    int fd,
    const Pickle& pickle,
    PickleIterator iter,
    const std::vector<base::ScopedFD*>& fds) {
  base::SharedMemoryCreateOptions options;
  uint32_t size;
  if (!pickle.ReadUInt32(&iter, &size))
    return;
  options.size = size;
  if (!pickle.ReadBool(&iter, &options.executable))
    return;
  int shm_fd = -1;
  base::SharedMemory shm;
  if (shm.Create(options))
    shm_fd = shm.handle().fd;
  Pickle reply;
  SendRendererReply(fds, reply, shm_fd);
}

void SandboxIPCHandler::HandleMatchWithFallback(
    int fd,
    const Pickle& pickle,
    PickleIterator iter,
    const std::vector<base::ScopedFD*>& fds) {
  std::string face;
  bool is_bold, is_italic;
  uint32 charset, fallback_family;

  if (!pickle.ReadString(&iter, &face) || face.empty() ||
      !pickle.ReadBool(&iter, &is_bold) ||
      !pickle.ReadBool(&iter, &is_italic) ||
      !pickle.ReadUInt32(&iter, &charset) ||
      !pickle.ReadUInt32(&iter, &fallback_family)) {
    return;
  }

  int font_fd = MatchFontFaceWithFallback(
      face, is_bold, is_italic, charset, fallback_family);

  Pickle reply;
  SendRendererReply(fds, reply, font_fd);

  if (font_fd >= 0) {
    if (IGNORE_EINTR(close(font_fd)) < 0)
      PLOG(ERROR) << "close";
  }
}

void SandboxIPCHandler::SendRendererReply(
    const std::vector<base::ScopedFD*>& fds,
    const Pickle& reply,
    int reply_fd) {
  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  struct iovec iov = {const_cast<void*>(reply.data()), reply.size()};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char control_buffer[CMSG_SPACE(sizeof(int))];

  if (reply_fd != -1) {
    struct stat st;
    if (fstat(reply_fd, &st) == 0 && S_ISDIR(st.st_mode)) {
      LOG(FATAL) << "Tried to send a directory descriptor over sandbox IPC";
      // We must never send directory descriptors to a sandboxed process
      // because they can use openat with ".." elements in the path in order
      // to escape the sandbox and reach the real filesystem.
    }

    struct cmsghdr* cmsg;
    msg.msg_control = control_buffer;
    msg.msg_controllen = sizeof(control_buffer);
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &reply_fd, sizeof(reply_fd));
    msg.msg_controllen = cmsg->cmsg_len;
  }

  if (HANDLE_EINTR(sendmsg(fds[0]->get(), &msg, MSG_DONTWAIT)) < 0)
    PLOG(ERROR) << "sendmsg";
}

SandboxIPCHandler::~SandboxIPCHandler() {
  paths_.deleteAll();
  if (webkit_platform_support_)
    blink::shutdownWithoutV8();

  if (IGNORE_EINTR(close(lifeline_fd_)) < 0)
    PLOG(ERROR) << "close";
  if (IGNORE_EINTR(close(browser_socket_)) < 0)
    PLOG(ERROR) << "close";
}

void SandboxIPCHandler::EnsureWebKitInitialized() {
  if (webkit_platform_support_)
    return;
  webkit_platform_support_.reset(new BlinkPlatformImpl);
  blink::initializeWithoutV8(webkit_platform_support_.get());
}

}  // namespace content
