// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_
#define CHROME_BROWSER_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_
#pragma once

#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/plugin_data_remover_helper.h"
#include "chrome/browser/webui/options/options_ui.h"

// Clear browser data handler page UI handler.
class ClearBrowserDataHandler : public OptionsPageUIHandler,
                                public BrowsingDataRemover::Observer {
 public:
  ClearBrowserDataHandler();
  virtual ~ClearBrowserDataHandler();

  // OptionsPageUIHandler implementation.
  virtual void Initialize();

  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Javascript callback to start clearing data.
  void HandleClearBrowserData(const ListValue* value);

  // Updates the UI to reflect whether clearing LSO data is supported.
  void UpdateClearPluginLSOData();

  // Callback from BrowsingDataRemover. Closes the dialog.
  virtual void OnBrowsingDataRemoverDone();

  // If non-null it means removal is in progress. BrowsingDataRemover takes care
  // of deleting itself when done.
  BrowsingDataRemover* remover_;

  // Used for asynchronously updating the preference stating whether clearing
  // LSO data is supported.
  PluginDataRemoverHelper clear_plugin_lso_data_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ClearBrowserDataHandler);
};

#endif  // CHROME_BROWSER_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_
