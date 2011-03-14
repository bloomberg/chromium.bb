// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_KEYWORD_EDITOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_KEYWORD_EDITOR_VIEW_H_
#pragma once

#include <Windows.h>

#include "base/string16.h"
#include "chrome/browser/search_engines/template_url_model_observer.h"
#include "chrome/browser/ui/search_engines/edit_search_engine_controller.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "views/controls/button/button.h"
#include "views/controls/table/table_view_observer.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Label;
class NativeButton;
}

namespace {
class BorderView;
}

class SearchEngineSelectionObserver;
class SkBitmap;
class TemplateURL;

// KeywordEditorView allows the user to edit keywords.

class KeywordEditorView : public views::View,
                          public views::TableViewObserver,
                          public views::ButtonListener,
                          public TemplateURLModelObserver,
                          public views::DialogDelegate,
                          public EditSearchEngineControllerDelegate {
 public:
  // Shows the KeywordEditorView for the specified profile. If there is a
  // KeywordEditorView already open, it is closed and a new one is shown.
  static void Show(Profile* profile);

  // Shows the KeywordEditorView for the specified profile, and passes in
  // an observer to be called back on view close.
  static void ShowAndObserve(Profile* profile,
                             SearchEngineSelectionObserver* observer);

  KeywordEditorView(Profile* profile,
                    SearchEngineSelectionObserver* observer);

  virtual ~KeywordEditorView();

  // Overridden from EditSearchEngineControllerDelegate.
  // Calls AddTemplateURL or ModifyTemplateURL as appropriate.
  virtual void OnEditedKeyword(const TemplateURL* template_url,
                               const string16& title,
                               const string16& keyword,
                               const std::string& url);

  // Overridden to invoke Layout.
  virtual gfx::Size GetPreferredSize();

  // views::DialogDelegate methods:
  virtual bool CanResize() const;
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetWindowName() const;
  virtual int GetDialogButtons() const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual views::View* GetContentsView();

 private:
  void Init();

  // Creates the layout and adds the views to it.
  void InitLayoutManager();

  // TableViewObserver method. Updates buttons contingent on the selection.
  virtual void OnSelectionChanged();
  // Edits the selected item.
  virtual void OnDoubleClick();

  // Button::ButtonListener method.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // TemplateURLModelObserver notification.
  virtual void OnTemplateURLModelChanged();

  // Toggles whether the selected keyword is the default search provider.
  void MakeDefaultTemplateURL();

  // The profile.
  Profile* profile_;

  // Observer gets a callback when the KeywordEditorView closes.
  SearchEngineSelectionObserver* observer_;

  scoped_ptr<KeywordEditorController> controller_;

  // True if the user has set a default search engine in this dialog.
  bool default_chosen_;

  // All the views are added as children, so that we don't need to delete
  // them directly.
  views::TableView* table_view_;
  views::NativeButton* add_button_;
  views::NativeButton* edit_button_;
  views::NativeButton* remove_button_;
  views::NativeButton* make_default_button_;

  DISALLOW_COPY_AND_ASSIGN(KeywordEditorView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_KEYWORD_EDITOR_VIEW_H_
