// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRASH_HANDLER_HOST_LINUX_H_
#define CHROME_BROWSER_CRASH_HANDLER_HOST_LINUX_H_
#pragma once

#include <string>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"

template <typename T> struct DefaultSingletonTraits;

namespace base {
class Thread;
}

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

#if defined(USE_LINUX_BREAKPAD)
  // Whether we are shutting down or not.
  bool IsShuttingDown() const;
#endif

 protected:
  CrashHandlerHostLinux();
  virtual ~CrashHandlerHostLinux();

#if defined(USE_LINUX_BREAKPAD)
  // Only called in concrete subclasses.
  void InitCrashUploaderThread();

  std::string process_type_;
#endif

 private:
  void Init();

#if defined(USE_LINUX_BREAKPAD)
  // This is here on purpose to make CrashHandlerHostLinux abstract.
  virtual void SetProcessType() = 0;
#endif

  int process_socket_;
  int browser_socket_;

#if defined(USE_LINUX_BREAKPAD)
  MessageLoopForIO::FileDescriptorWatcher file_descriptor_watcher_;
  scoped_ptr<base::Thread> uploader_thread_;
  bool shutting_down_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CrashHandlerHostLinux);
};

class GpuCrashHandlerHostLinux : public CrashHandlerHostLinux {
 public:
  // Returns the singleton instance.
  static GpuCrashHandlerHostLinux* GetInstance();

 private:
  friend struct DefaultSingletonTraits<GpuCrashHandlerHostLinux>;
  GpuCrashHandlerHostLinux();
  virtual ~GpuCrashHandlerHostLinux();

#if defined(USE_LINUX_BREAKPAD)
  virtual void SetProcessType();
#endif

  DISALLOW_COPY_AND_ASSIGN(GpuCrashHandlerHostLinux);
};

class PluginCrashHandlerHostLinux : public CrashHandlerHostLinux {
 public:
  // Returns the singleton instance.
  static PluginCrashHandlerHostLinux* GetInstance();

 private:
  friend struct DefaultSingletonTraits<PluginCrashHandlerHostLinux>;
  PluginCrashHandlerHostLinux();
  virtual ~PluginCrashHandlerHostLinux();

#if defined(USE_LINUX_BREAKPAD)
  virtual void SetProcessType();
#endif

  DISALLOW_COPY_AND_ASSIGN(PluginCrashHandlerHostLinux);
};

class RendererCrashHandlerHostLinux : public CrashHandlerHostLinux {
 public:
  // Returns the singleton instance.
  static RendererCrashHandlerHostLinux* GetInstance();

 private:
  friend struct DefaultSingletonTraits<RendererCrashHandlerHostLinux>;
  RendererCrashHandlerHostLinux();
  virtual ~RendererCrashHandlerHostLinux();

#if defined(USE_LINUX_BREAKPAD)
  virtual void SetProcessType();
#endif

  DISALLOW_COPY_AND_ASSIGN(RendererCrashHandlerHostLinux);
};

#endif  // CHROME_BROWSER_CRASH_HANDLER_HOST_LINUX_H_
