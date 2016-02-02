// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_STARTUP_PAGES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_STARTUP_PAGES_HANDLER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "components/omnibox/browser/autocomplete_controller_delegate.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "ui/base/models/table_model_observer.h"

class AutocompleteController;
class CustomHomePagesTableModel;
class TemplateURLService;

namespace options {

// Chrome browser options page UI handler.
class StartupPagesHandler : public OptionsPageUIHandler,
                            public AutocompleteControllerDelegate,
                            public ui::TableModelObserver {
 public:
  StartupPagesHandler();
  ~StartupPagesHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializeHandler() override;
  void InitializePage() override;
  void RegisterMessages() override;

  // AutocompleteControllerDelegate implementation.
  void OnResultChanged(bool default_match_changed) override;

  // ui::TableModelObserver implementation.
  void OnModelChanged() override;
  void OnItemsChanged(int start, int length) override;
  void OnItemsAdded(int start, int length) override;
  void OnItemsRemoved(int start, int length) override;

 private:
  // Saves the changes that have been made. Called from WebUI.
  void CommitChanges(const base::ListValue* args);

  // Cancels the changes that have been made. Called from WebUI.
  void CancelChanges(const base::ListValue* args);

  // Removes the startup page at the given indexes. Called from WebUI.
  void RemoveStartupPages(const base::ListValue* args);

  // Adds a startup page with the given URL after the given index.
  // Called from WebUI.
  void AddStartupPage(const base::ListValue* args);

  // Changes the startup page at the given index to the given URL.
  // Called from WebUI.
  void EditStartupPage(const base::ListValue* args);

  // Sets the startup page set to the current pages. Called from WebUI.
  void SetStartupPagesToCurrentPages(const base::ListValue* args);

  // Writes the current set of startup pages to prefs. Called from WebUI.
  void DragDropStartupPage(const base::ListValue* args);

  // Gets autocomplete suggestions asychronously for the given string.
  // Called from WebUI.
  void RequestAutocompleteSuggestions(const base::ListValue* args);

  // Loads the current set of custom startup pages and reports it to the WebUI.
  void UpdateStartupPages();

  // Writes the current set of startup pages to prefs.
  void SaveStartupPagesPref();

  scoped_ptr<AutocompleteController> autocomplete_controller_;

  // Used to observe updates to the preference of the list of URLs to load
  // on startup, which can be updated via sync.
  PrefChangeRegistrar pref_change_registrar_;

  // The set of pages to launch on startup.
  scoped_ptr<CustomHomePagesTableModel> startup_custom_pages_table_model_;

  DISALLOW_COPY_AND_ASSIGN(StartupPagesHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_STARTUP_PAGES_HANDLER_H_
