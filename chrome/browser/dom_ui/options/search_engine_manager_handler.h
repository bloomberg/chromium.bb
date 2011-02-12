// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_OPTIONS_SEARCH_ENGINE_MANAGER_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_OPTIONS_SEARCH_ENGINE_MANAGER_HANDLER_H_

#include "chrome/browser/dom_ui/options/options_ui.h"
#include "chrome/browser/search_engines/edit_search_engine_controller.h"
#include "ui/base/models/table_model_observer.h"

class KeywordEditorController;

class SearchEngineManagerHandler : public OptionsPageUIHandler,
                                   public ui::TableModelObserver,
                                   public EditSearchEngineControllerDelegate {
 public:
  SearchEngineManagerHandler();
  virtual ~SearchEngineManagerHandler();

  virtual void Initialize();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // ui::TableModelObserver implementation.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

  // EditSearchEngineControllerDelegate implementation.
  virtual void OnEditedKeyword(const TemplateURL* template_url,
                               const string16& title,
                               const string16& keyword,
                               const std::string& url);

  virtual void RegisterMessages();

 private:
  scoped_ptr<KeywordEditorController> list_controller_;
  scoped_ptr<EditSearchEngineController> edit_controller_;

  // Removes the search engine at the given index. Called from WebUI.
  void RemoveSearchEngine(const ListValue* args);

  // Sets the search engine at the given index to be default. Called from WebUI.
  void SetDefaultSearchEngine(const ListValue* args);

  // Starts an edit session for the search engine at the given index. If the
  // index is -1, starts editing a new search engine instead of an existing one.
  // Called from WebUI.
  void EditSearchEngine(const ListValue* args);

  // Validates the given search engine values, and reports the results back
  // to WebUI. Called from WebUI.
  void CheckSearchEngineInfoValidity(const ListValue* args);

  // Called when an edit is cancelled.
  // Called from WebUI.
  void EditCancelled(const ListValue* args);

  // Called when an edit is finished and should be saved.
  // Called from WebUI.
  void EditCompleted(const ListValue* args);

  // Returns a dictionary to pass to WebUI representing the given search engine.
  DictionaryValue* CreateDictionaryForEngine(int index, bool is_default);

  DISALLOW_COPY_AND_ASSIGN(SearchEngineManagerHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_OPTIONS_SEARCH_ENGINE_MANAGER_HANDLER_H_
