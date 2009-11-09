// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRASH_HANDLER_HOST_LINUX_H_
#define CHROME_BROWSER_CRASH_HANDLER_HOST_LINUX_H_

#include <string>

#include "base/singleton.h"
#include "base/message_loop.h"

// This is the base class for singleton objects which crash dump renderers and
// plugins on Linux. We perform the crash dump from the browser because it
// allows us to be outside the sandbox.
//
// PluginCrashHandlerHostLinux and RendererCrashHandlerHostLinux are singletons
// that handle plugin and renderer crashes, respectively.
//
// Processes signal that they need to be dumped by sending a datagram over a
// UNIX domain socket. All processes of the same type share the client end of
// this socket which is installed in their descriptor table before exec.
class CrashHandlerHostLinux : public MessageLoopForIO::Watcher,
                              public MessageLoop::DestructionObserver {
 public:
  // Get the file descriptor which processes should be given in order to signal
  // crashes to the browser.
  int GetDeathSignalSocket() const {
    return process_socket_;
  }

  // MessagePumbLibevent::Watcher impl:
  virtual void OnFileCanWriteWithoutBlocking(int fd);
  virtual void OnFileCanReadWithoutBlocking(int fd);

  // MessageLoop::DestructionObserver impl:
  virtual void WillDestroyCurrentMessageLoop();

 protected:
  CrashHandlerHostLinux();
  ~CrashHandlerHostLinux();
  // This is here on purpose to make CrashHandlerHostLinux abstract.
  virtual void SetProcessType() = 0;

  std::string process_type_;

 private:
  void Init();

  int process_socket_;
  int browser_socket_;
  MessageLoopForIO::FileDescriptorWatcher file_descriptor_watcher_;

  DISALLOW_COPY_AND_ASSIGN(CrashHandlerHostLinux);
};

class PluginCrashHandlerHostLinux : public CrashHandlerHostLinux {
 private:
  friend struct DefaultSingletonTraits<PluginCrashHandlerHostLinux>;
  PluginCrashHandlerHostLinux() {
    SetProcessType();
  }
  ~PluginCrashHandlerHostLinux() {}

  virtual void SetProcessType() {
    process_type_ = "plugin";
  }

  DISALLOW_COPY_AND_ASSIGN(PluginCrashHandlerHostLinux);
};

class RendererCrashHandlerHostLinux : public CrashHandlerHostLinux {
 private:
  friend struct DefaultSingletonTraits<RendererCrashHandlerHostLinux>;
  RendererCrashHandlerHostLinux() {
    SetProcessType();
  }
  ~RendererCrashHandlerHostLinux() {}

  virtual void SetProcessType() {
    process_type_ = "renderer";
  }

  DISALLOW_COPY_AND_ASSIGN(RendererCrashHandlerHostLinux);
};

#endif  // CHROME_BROWSER_CRASH_HANDLER_HOST_LINUX_H_
