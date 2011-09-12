// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_CONTENT_PLUGIN_CLIENT_H_
#define CONTENT_SHELL_SHELL_CONTENT_PLUGIN_CLIENT_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/plugin/content_plugin_client.h"

namespace content {

class ShellContentPluginClient : public ContentPluginClient {
 public:
  virtual ~ShellContentPluginClient();
  virtual void PluginProcessStarted(const string16& plugin_name) OVERRIDE;
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_CONTENT_PLUGIN_CLIENT_H_
