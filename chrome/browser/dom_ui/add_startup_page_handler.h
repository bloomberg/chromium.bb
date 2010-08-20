// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_ADD_STARTUP_PAGE_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_ADD_STARTUP_PAGE_HANDLER_H_
#pragma once

#include "app/table_model_observer.h"
#include "base/basictypes.h"
#include "chrome/browser/dom_ui/options_ui.h"

class PossibleURLModel;

// Chrome personal options page UI handler.
class AddStartupPageHandler : public OptionsPageUIHandler,
                              public TableModelObserver {
 public:
  AddStartupPageHandler();
  virtual ~AddStartupPageHandler();

  // OptionsPageUIHandler implementation.
  virtual void Initialize();
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void RegisterMessages();

  // TableModelObserver implementation.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

 private:
  // Request to update the text field with the URL of the recent page at the
  // given index, formatted for user input. Called from DOMUI.
  void UpdateFieldWithRecentPage(const ListValue* args);

  scoped_ptr<PossibleURLModel> url_table_model_;

  DISALLOW_COPY_AND_ASSIGN(AddStartupPageHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_ADD_STARTUP_PAGE_HANDLER_H_
