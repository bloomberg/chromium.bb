// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_OPTIONS_SYNC_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_OPTIONS_SYNC_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/dom_ui/options/options_ui.h"

// Chrome sync options page UI handler.
// TODO(jhawkins): Move the meat of this class to PersonalOptionsHandler.
class SyncOptionsHandler : public OptionsPageUIHandler {
 public:
  SyncOptionsHandler();
  virtual ~SyncOptionsHandler();

  // OptionsUIHandler implementation.
  virtual bool IsEnabled();
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void Initialize();
  virtual void RegisterMessages();

 private:
  // Called when the user updates the set of enabled data types to sync. |args|
  // is ignored.
  void OnPreferredDataTypesUpdated(const ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(SyncOptionsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_OPTIONS_SYNC_OPTIONS_HANDLER_H_
