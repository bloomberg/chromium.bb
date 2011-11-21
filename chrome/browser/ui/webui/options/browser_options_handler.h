// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/search_engines/template_url_service_observer.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "ui/base/models/table_model_observer.h"

class AutocompleteController;
class CustomHomePagesTableModel;
class TemplateURLService;

// Chrome browser options page UI handler.
class BrowserOptionsHandler : public OptionsPageUIHandler,
                              public AutocompleteControllerDelegate,
                              public ShellIntegration::DefaultWebClientObserver,
                              public TemplateURLServiceObserver,
                              public ui::TableModelObserver {
 public:
  BrowserOptionsHandler();
  virtual ~BrowserOptionsHandler();

  virtual void Initialize() OVERRIDE;

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // AutocompleteControllerDelegate implementation.
  virtual void OnResultChanged(bool default_match_changed) OVERRIDE;

  // ShellIntegration::DefaultWebClientObserver implementation.
  virtual void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) OVERRIDE;

  // TemplateURLServiceObserver implementation.
  virtual void OnTemplateURLServiceChanged() OVERRIDE;

  // ui::TableModelObserver implementation.
  virtual void OnModelChanged() OVERRIDE;
  virtual void OnItemsChanged(int start, int length) OVERRIDE;
  virtual void OnItemsAdded(int start, int length) OVERRIDE;
  virtual void OnItemsRemoved(int start, int length) OVERRIDE;

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Makes this the default browser. Called from WebUI.
  void BecomeDefaultBrowser(const ListValue* args);

  // Sets the search engine at the given index to be default. Called from WebUI.
  void SetDefaultSearchEngine(const ListValue* args);

  // Removes the startup page at the given indexes. Called from WebUI.
  void RemoveStartupPages(const ListValue* args);

  // Adds a startup page with the given URL after the given index.
  // Called from WebUI.
  void AddStartupPage(const ListValue* args);

  // Changes the startup page at the given index to the given URL.
  // Called from WebUI.
  void EditStartupPage(const ListValue* args);

  // Sets the startup page set to the current pages. Called from WebUI.
  void SetStartupPagesToCurrentPages(const ListValue* args);

  // Writes the current set of startup pages to prefs. Called from WebUI.
  void DragDropStartupPage(const ListValue* args);

  // Gets autocomplete suggestions asychronously for the given string.
  // Called from WebUI.
  void RequestAutocompleteSuggestions(const ListValue* args);

  // Enables/disables Instant.
  void EnableInstant(const ListValue* args);
  void DisableInstant(const ListValue* args);

  // Called to request information about the Instant field trial.
  void GetInstantFieldTrialStatus(const ListValue* args);

  // Returns the string ID for the given default browser state.
  int StatusStringIdForState(ShellIntegration::DefaultWebClientState state);

  // Gets the current default browser state, and asynchronously reports it to
  // the WebUI page.
  void UpdateDefaultBrowserState();

  // Updates the UI with the given state for the default browser.
  void SetDefaultBrowserUIString(int status_string_id);

  // Loads the current set of custom startup pages and reports it to the WebUI.
  void UpdateStartupPages();

  // Loads the possible default search engine list and reports it to the WebUI.
  void UpdateSearchEngines();

  // Writes the current set of startup pages to prefs.
  void SaveStartupPagesPref();

  scoped_refptr<ShellIntegration::DefaultBrowserWorker>
      default_browser_worker_;

  StringPrefMember homepage_;
  BooleanPrefMember default_browser_policy_;

  // Used to observe updates to the preference of the list of URLs to load
  // on startup, which can be updated via sync.
  PrefChangeRegistrar pref_change_registrar_;

  TemplateURLService* template_url_service_;  // Weak.

  // TODO(stuartmorgan): Once there are no other clients of
  // CustomHomePagesTableModel, consider changing it to something more like
  // TemplateURLService.
  scoped_ptr<CustomHomePagesTableModel> startup_custom_pages_table_model_;

  scoped_ptr<AutocompleteController> autocomplete_controller_;

  DISALLOW_COPY_AND_ASSIGN(BrowserOptionsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_BROWSER_OPTIONS_HANDLER_H_
