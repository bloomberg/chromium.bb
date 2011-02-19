// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// http://code.google.com/p/chromium/wiki/LinuxSandboxIPC

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_SANDBOX_HOST_LINUX_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_SANDBOX_HOST_LINUX_H_
#pragma once

#include <string>

#include "base/logging.h"

template <typename T> struct DefaultSingletonTraits;

// This is a singleton object which handles sandbox requests from the
// renderers.
class RenderSandboxHostLinux {
 public:
  // Returns the singleton instance.
  static RenderSandboxHostLinux* GetInstance();

  // Get the file descriptor which renderers should be given in order to signal
  // crashes to the browser.
  int GetRendererSocket() const {
    DCHECK(initialized_);
    return renderer_socket_;
  }
  pid_t pid() const {
    DCHECK(initialized_);
    return pid_;
  }
  void Init(const std::string& sandbox_path);

 private:
  friend struct DefaultSingletonTraits<RenderSandboxHostLinux>;
  // This object must be constructed on the main thread.
  RenderSandboxHostLinux();
  ~RenderSandboxHostLinux();

  // Whether Init() has been called yet.
  bool initialized_;

  int renderer_socket_;
  int childs_lifeline_fd_;
  pid_t pid_;

  DISALLOW_COPY_AND_ASSIGN(RenderSandboxHostLinux);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_SANDBOX_HOST_LINUX_H_
