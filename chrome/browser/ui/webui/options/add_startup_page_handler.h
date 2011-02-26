// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_ADD_STARTUP_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_ADD_STARTUP_PAGE_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "ui/base/models/table_model_observer.h"

class PossibleURLModel;

// Chrome personal options page UI handler.
class AddStartupPageHandler : public OptionsPageUIHandler,
                              public ui::TableModelObserver {
 public:
  AddStartupPageHandler();
  virtual ~AddStartupPageHandler();

  // OptionsPageUIHandler implementation.
  virtual void Initialize();
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void RegisterMessages();

  // ui::TableModelObserver implementation.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

 private:
  // Request to update the text field with the URL of the recent page at the
  // given index, formatted for user input. Called from WebUI.
  void UpdateFieldWithRecentPage(const ListValue* args);

  scoped_ptr<PossibleURLModel> url_table_model_;

  DISALLOW_COPY_AND_ASSIGN(AddStartupPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_ADD_STARTUP_PAGE_HANDLER_H_
