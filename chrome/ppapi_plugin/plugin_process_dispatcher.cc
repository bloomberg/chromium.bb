// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/ppapi_plugin/plugin_process_dispatcher.h"

#include "chrome/common/child_process.h"

namespace {

class PluginReleaseTask : public Task {
 public:
  void Run() {
    ChildProcess::current()->ReleaseProcess();
  }
};

// How long we wait before releasing the plugin process.
const int kPluginReleaseTimeMs = 30 * 1000;  // 30 seconds.

}  // namespace

PluginProcessDispatcher::PluginProcessDispatcher(
    base::ProcessHandle remote_process_handle,
    GetInterfaceFunc get_interface)
    : pp::proxy::PluginDispatcher(remote_process_handle, get_interface) {
  ChildProcess::current()->AddRefProcess();
}

PluginProcessDispatcher::~PluginProcessDispatcher() {
  // Don't free the process right away. This timer allows the child process
  // to be re-used if the user rapidly goes to a new page that requires this
  // plugin. This is the case for common plugins where they may be used on a
  // source and destination page of a navigation. We don't want to tear down
  // and re-start processes each time in these cases.
  MessageLoop::current()->PostDelayedTask(FROM_HERE, new PluginReleaseTask(),
                                          kPluginReleaseTimeMs);
}
