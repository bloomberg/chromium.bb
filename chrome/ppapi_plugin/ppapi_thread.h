// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PPAPI_PLUGIN_PPAPI_THREAD_H_
#define CHROME_PPAPI_PLUGIN_PPAPI_THREAD_H_
#pragma once

#include "base/basictypes.h"
#include "base/process.h"
#include "base/scoped_native_library.h"
#include "base/scoped_ptr.h"
#include "build/build_config.h"
#include "chrome/common/child_thread.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/proxy/dispatcher.h"

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
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // Message handlers.
  void OnMsgLoadPlugin(const FilePath& path);
  void OnMsgCreateChannel(base::ProcessHandle host_process_handle,
                          int renderer_id);

  // Sets up the channel to the given renderer. On success, returns true and
  // fills the given ChannelHandle with the information from the new channel.
  bool SetupRendererChannel(base::ProcessHandle host_process_handle,
                            int renderer_id,
                            IPC::ChannelHandle* handle);

  base::ScopedNativeLibrary library_;

  pp::proxy::Dispatcher::GetInterfaceFunc get_plugin_interface_;

  // Local concept of the module ID. Some functions take this. It's necessary
  // for the in-process PPAPI to handle this properly, but for proxied it's
  // unnecessary. The proxy talking to multiple renderers means that each
  // renderer has a different idea of what the module ID is for this plugin.
  // To force people to "do the right thing" we generate a random module ID
  // and pass it around as necessary.
  PP_Module local_pp_module_;

  DISALLOW_COPY_AND_ASSIGN(PpapiThread);
};

#endif  // CHROME_PPAPI_PLUGIN_PPAPI_THREAD_H_
