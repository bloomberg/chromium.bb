// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_PLUGIN_THREAD_H_
#define CHROME_PLUGIN_PLUGIN_THREAD_H_
#pragma once

#include "base/file_path.h"
#include "base/native_library.h"
#include "build/build_config.h"
#include "chrome/plugin/plugin_channel.h"
#include "content/common/child_thread.h"
#include "webkit/plugins/npapi/plugin_lib.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

// The PluginThread class represents a background thread where plugin instances
// live.  Communication occurs between WebPluginDelegateProxy in the renderer
// process and WebPluginDelegateStub in this thread through IPC messages.
class PluginThread : public ChildThread {
 public:
  PluginThread();
  ~PluginThread();

  // Returns the one plugin thread.
  static PluginThread* current();

  FilePath plugin_path() { return plugin_path_; }

 private:
  virtual bool OnControlMessageReceived(const IPC::Message& msg);

  // Callback for when a channel has been created.
  void OnCreateChannel(int renderer_id, bool incognito);
  void OnPluginMessage(const std::vector<uint8> &data);
  void OnNotifyRenderersOfPendingShutdown();
#if defined(OS_MACOSX)
  void OnAppActivated();
  void OnPluginFocusNotify(uint32 instance_id);
#endif

  // The plugin module which is preloaded in Init
  base::NativeLibrary preloaded_plugin_module_;

  // Points to the plugin file that this process hosts.
  FilePath plugin_path_;

  DISALLOW_COPY_AND_ASSIGN(PluginThread);
};

#endif  // CHROME_PLUGIN_PLUGIN_THREAD_H_
