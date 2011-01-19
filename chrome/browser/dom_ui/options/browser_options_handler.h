// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/dom_ui/options/options_ui.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/search_engines/template_url_model_observer.h"
#include "chrome/browser/shell_integration.h"
#include "ui/base/models/table_model_observer.h"

class CustomHomePagesTableModel;
class OptionsManagedBannerHandler;
class StringPrefMember;
class TemplateURLModel;

// Chrome browser options page UI handler.
class BrowserOptionsHandler : public OptionsPageUIHandler,
                              public ShellIntegration::DefaultBrowserObserver,
                              public TemplateURLModelObserver,
                              public ui::TableModelObserver {
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

  // ui::TableModelObserver implementation.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

 private:
  // Sets the home page to the given string. Called from DOMUI.
  void SetHomePage(const ListValue* args);

  // Makes this the default browser. Called from DOMUI.
  void BecomeDefaultBrowser(const ListValue* args);

  // Sets the search engine at the given index to be default. Called from DOMUI.
  void SetDefaultSearchEngine(const ListValue* args);

  // Removes the startup page at the given indexes. Called from DOMUI.
  void RemoveStartupPages(const ListValue* args);

  // Adds a startup page with the given URL after the given index.
  // Called from DOMUI.
  void AddStartupPage(const ListValue* args);

  // Changes the startup page at the given index to the given URL.
  // Called from DOMUI.
  void EditStartupPage(const ListValue* args);

  // Sets the startup page set to the current pages. Called from DOMUI.
  void SetStartupPagesToCurrentPages(const ListValue* args);

  // Returns the string ID for the given default browser state.
  int StatusStringIdForState(ShellIntegration::DefaultBrowserState state);

  // Gets the current default browser state, and asynchronously reports it to
  // the DOMUI page.
  void UpdateDefaultBrowserState();

  // Updates the UI with the given state for the default browser.
  void SetDefaultBrowserUIString(int status_string_id);

  // Loads the current set of custom startup pages and reports it to the DOMUI.
  void UpdateStartupPages();

  // Loads the possible default search engine list and reports it to the DOMUI.
  void UpdateSearchEngines();

  // Writes the current set of startup pages to prefs.
  void SaveStartupPagesPref();

  scoped_refptr<ShellIntegration::DefaultBrowserWorker> default_browser_worker_;

  StringPrefMember homepage_;

  TemplateURLModel* template_url_model_;  // Weak.

  // TODO(stuartmorgan): Once there are no other clients of
  // CustomHomePagesTableModel, consider changing it to something more like
  // TemplateURLModel.
  scoped_ptr<CustomHomePagesTableModel> startup_custom_pages_table_model_;
  scoped_ptr<OptionsManagedBannerHandler> banner_handler_;

  DISALLOW_COPY_AND_ASSIGN(BrowserOptionsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_
