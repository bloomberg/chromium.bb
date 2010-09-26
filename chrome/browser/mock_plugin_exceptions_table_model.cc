// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mock_plugin_exceptions_table_model.h"

#include "chrome/common/plugin_group.h"

void MockPluginExceptionsTableModel::set_plugins(
    const PluginUpdater::PluginMap& plugins) {
  plugins_ = plugins;
}

void MockPluginExceptionsTableModel::GetPlugins(
    PluginUpdater::PluginMap* plugins) {
  *plugins = plugins_;
}
