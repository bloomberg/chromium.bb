// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mock_plugin_exceptions_table_model.h"

#include "webkit/glue/plugins/webplugininfo.h"

void MockPluginExceptionsTableModel::set_plugins(
    const std::vector<WebPluginInfo>& plugins) {
  plugins_ = plugins;
}

void MockPluginExceptionsTableModel::GetPlugins(
    std::vector<WebPluginInfo>* plugins) {
  *plugins = plugins_;
}
