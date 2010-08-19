// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_CLEAR_BROWSER_DATA_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_CLEAR_BROWSER_DATA_HANDLER_H_
#pragma once

#include "chrome/browser/dom_ui/options_ui.h"
#include "chrome/browser/browsing_data_remover.h"

// Clear browser data handler page UI handler.
class ClearBrowserDataHandler : public OptionsPageUIHandler,
                                public BrowsingDataRemover::Observer {
 public:
  ClearBrowserDataHandler();
  virtual ~ClearBrowserDataHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  void HandleClearBrowserData(const ListValue* value);

  // Callback from BrowsingDataRemover. Closes the dialog.
  virtual void OnBrowsingDataRemoverDone();

  // If non-null it means removal is in progress. BrowsingDataRemover takes care
  // of deleting itself when done.
  BrowsingDataRemover* remover_;

  DISALLOW_COPY_AND_ASSIGN(ClearBrowserDataHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_CLEAR_BROWSER_DATA_HANDLER_H_
