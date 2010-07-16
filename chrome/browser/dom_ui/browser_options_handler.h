// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_BROWSER_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_BROWSER_OPTIONS_HANDLER_H_

#include "chrome/browser/dom_ui/options_ui.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/shell_integration.h"

// Chrome browser options page UI handler.
class BrowserOptionsHandler : public OptionsPageUIHandler,
                              public ShellIntegration::DefaultBrowserObserver,
                              public TemplateURLModelObserver {
 public:
  BrowserOptionsHandler();
  virtual ~BrowserOptionsHandler();

  virtual void Initialize();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void RegisterMessages();

  // ShellIntegration::DefaultBrowserObserver implementation.
  virtual void SetDefaultBrowserUIState(
      ShellIntegration::DefaultBrowserUIState state);

  // TemplateURLModelObserver implementation.
  virtual void OnTemplateURLModelChanged();

 private:
  // Makes this the default browser. Called from DOMUI.
  void BecomeDefaultBrowser(const Value* value);

  // Sets the search engine at the given index to be default. Called from DOMUI.
  void SetDefaultSearchEngine(const Value* value);

  // Returns the string ID for the given default browser state.
  int StatusStringIdForState(ShellIntegration::DefaultBrowserState state);

  // Gets the current default browser state, and asynchronously reports it to
  // the DOMUI page.
  void UpdateDefaultBrowserState();

  // Updates the UI with the given state for the default browser.
  void SetDefaultBrowserUIString(int status_string_id);

  scoped_refptr<ShellIntegration::DefaultBrowserWorker> default_browser_worker_;

  TemplateURLModel* template_url_model_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(BrowserOptionsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_BROWSER_OPTIONS_HANDLER_H_
