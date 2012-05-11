// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/plugin_process_dispatcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/common/child_process.h"

namespace {

// How long we wait before releasing the plugin process.
const int kPluginReleaseTimeSeconds = 30;

}  // namespace

PluginProcessDispatcher::PluginProcessDispatcher(
    PP_GetInterface_Func get_interface,
    bool incognito)
    : ppapi::proxy::PluginDispatcher(get_interface,
                                     incognito) {
  ChildProcess::current()->AddRefProcess();
}

PluginProcessDispatcher::~PluginProcessDispatcher() {
  // Don't free the process right away. This timer allows the child process
  // to be re-used if the user rapidly goes to a new page that requires this
  // plugin. This is the case for common plugins where they may be used on a
  // source and destination page of a navigation. We don't want to tear down
  // and re-start processes each time in these cases.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ChildProcess::ReleaseProcess,
                 base::Unretained(ChildProcess::current())),
      base::TimeDelta::FromSeconds(kPluginReleaseTimeSeconds));
}
