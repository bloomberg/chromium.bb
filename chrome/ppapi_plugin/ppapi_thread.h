// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PPAPI_PLUGIN_PPAPI_THREAD_H_
#define CHROME_PPAPI_PLUGIN_PPAPI_THREAD_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_native_library.h"
#include "base/scoped_ptr.h"
#include "build/build_config.h"
#include "chrome/common/child_thread.h"

class FilePath;

namespace IPC {
struct ChannelHandle;
}

namespace pp {
namespace proxy {
class PluginDispatcher;
}
}

class PpapiThread : public ChildThread {
 public:
  PpapiThread();
  ~PpapiThread();

 private:
  // ChildThread overrides.
  virtual void OnMessageReceived(const IPC::Message& msg);

  // Message handlers.
  void OnLoadPlugin(const FilePath& path, int renderer_id);

  bool LoadPluginLib(const FilePath& path);

  // Sets up the channel to the given renderer. On success, returns true and
  // fills the given ChannelHandle with the information from the new channel.
  bool SetupRendererChannel(int renderer_id,
                            IPC::ChannelHandle* handle);

#if defined(OS_POSIX)
  // Close the plugin process' copy of the renderer's side of the plugin
  // channel.  This can be called after the renderer is known to have its own
  // copy of renderer_fd_.
  void CloseRendererFD();
#endif

  base::ScopedNativeLibrary library_;

  scoped_ptr<pp::proxy::PluginDispatcher> dispatcher_;

#if defined(OS_POSIX)
  // FD for the renderer end of the socket. It is closed when the IPC layer
  // indicates that the channel is connected, proving that the renderer has
  // access to its side of the socket.
  int renderer_fd_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PpapiThread);
};

#endif  // CHROME_PPAPI_PLUGIN_PPAPI_THREAD_H_
