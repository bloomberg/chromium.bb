// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_STARTUP_PAGES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_STARTUP_PAGES_HANDLER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/custom_home_pages_table_model.h"
#include "chrome/browser/ui/webui/settings/md_settings_ui.h"
#include "ui/base/models/table_model_observer.h"

namespace base {
class ListValue;
}

namespace content {
class WebUI;
}

namespace settings {

// Chrome browser startup settings handler.
class StartupPagesHandler : public SettingsPageUIHandler,
                            public ui::TableModelObserver {
 public:
  explicit StartupPagesHandler(content::WebUI* webui);
  ~StartupPagesHandler() override;

  // OptionsPageUIHandler:
  void RegisterMessages() override;

  // ui::TableModelObserver:
  void OnModelChanged() override;
  void OnItemsChanged(int start, int length) override;
  void OnItemsAdded(int start, int length) override;
  void OnItemsRemoved(int start, int length) override;

 private:
  // Sets the startup page set to the current pages. Called from WebUI.
  void SetStartupPagesToCurrentPages(const base::ListValue* args);

  CustomHomePagesTableModel startup_custom_pages_table_model_;

  DISALLOW_COPY_AND_ASSIGN(StartupPagesHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_STARTUP_PAGES_HANDLER_H_
