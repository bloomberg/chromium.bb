// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_SEARCH_ENGINE_MANAGER_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_SEARCH_ENGINE_MANAGER_HANDLER_H_

#include "chrome/browser/dom_ui/options_ui.h"

class SearchEngineManagerHandler : public OptionsPageUIHandler {
 public:
  SearchEngineManagerHandler();
  virtual ~SearchEngineManagerHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  virtual void RegisterMessages();

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchEngineManagerHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_SEARCH_ENGINE_MANAGER_HANDLER_H_
