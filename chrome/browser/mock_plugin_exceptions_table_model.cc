// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mock_plugin_exceptions_table_model.h"

MockPluginExceptionsTableModel::MockPluginExceptionsTableModel(
    HostContentSettingsMap* map,
    HostContentSettingsMap* otr_map)
    : PluginExceptionsTableModel(map, otr_map) {}

MockPluginExceptionsTableModel::~MockPluginExceptionsTableModel() {}

void MockPluginExceptionsTableModel::set_plugins(
    std::vector<webkit::npapi::PluginGroup>& plugins) {
  plugins_ = plugins;
}

void MockPluginExceptionsTableModel::GetPlugins(
    std::vector<webkit::npapi::PluginGroup>* plugin_groups) {
  *plugin_groups = plugins_;
}
