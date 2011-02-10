// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MOCK_PLUGIN_EXCEPTIONS_TABLE_MODEL_H_
#define CHROME_BROWSER_MOCK_PLUGIN_EXCEPTIONS_TABLE_MODEL_H_
#pragma once

#include <vector>

#include "chrome/browser/plugin_exceptions_table_model.h"

class MockPluginExceptionsTableModel : public PluginExceptionsTableModel {
 public:
  MockPluginExceptionsTableModel(HostContentSettingsMap* map,
                                 HostContentSettingsMap* otr_map);
  virtual ~MockPluginExceptionsTableModel();

  void set_plugins(std::vector<webkit::npapi::PluginGroup>& plugins);

 protected:
  virtual void GetPlugins(
      std::vector<webkit::npapi::PluginGroup>* plugin_groups);

 private:
  std::vector<webkit::npapi::PluginGroup> plugins_;
};

#endif  // CHROME_BROWSER_MOCK_PLUGIN_EXCEPTIONS_TABLE_MODEL_H_
