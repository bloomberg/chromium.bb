// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FIRST_RUN_SEARCH_ENGINE_VIEW_H_
#define CHROME_BROWSER_VIEWS_FIRST_RUN_SEARCH_ENGINE_VIEW_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/views/keyword_editor_view.h"
#include "gfx/size.h"
#include "views/controls/button/native_button.h"
#include "views/controls/link.h"
#include "views/view.h"
#include "views/window/window_delegate.h"

namespace views {
class ButtonListener;
class Label;
class Separator;
class Window;
}

class Profile;
class TemplateURL;
class TemplateURLModel;

// This class holds the logo and TemplateURL for a search engine and serves
// as its button in the search engine selection view.
class SearchEngineChoice : public views::NativeButton {
 public:
  // |listener| is the FirstRunView that waits for the search engine selection
  // to complete; |search_engine| holds the data for the particular search
  // engine this button represents; |use_small_logos| is true if we're
  // displaying more than three choices.
  SearchEngineChoice(views::ButtonListener* listener,
                     const TemplateURL* search_engine,
                     bool use_small_logos);

  virtual ~SearchEngineChoice() {}

  // These methods return data about the logo or text view associated
  // with this search engine choice.
  views::View* GetView() { return choice_view_.get(); }
  int GetChoiceViewWidth();
  int GetChoiceViewHeight();

  // Set the bounds for the search engine choice view; called in the
  // Layout method, when we know what the new bounds should be.
  void SetChoiceViewBounds(int x, int y, int width, int height);

  // Accessor for the search engine data this button represents.
  const TemplateURL* GetSearchEngine() { return search_engine_; }

 private:
  // Either an ImageView of a logo, or a Label with text.
  scoped_ptr<views::View> choice_view_;

  // True if choice_view_ is holding an ImageView.
  bool is_image_label_;

  // Data for the search engine held here.
  const TemplateURL* search_engine_;

  DISALLOW_COPY_AND_ASSIGN(SearchEngineChoice);
};

// This class displays a large search engine choice dialog view during
// initial first run import.
class FirstRunSearchEngineView
    : public views::View,
      public views::ButtonListener,
      public views::LinkController,
      public views::WindowDelegate,
      public KeywordEditorViewObserver,
      public TemplateURLModelObserver {
 public:
  // This class receives a callback when the search engine dialog closes.
  class SearchEngineViewObserver {
   public:
     virtual ~SearchEngineViewObserver() {}
    // Called when the user has chosen a search engine. If the user closes
    // the dialog without providing us with a search engine (because the search
    // engine has been chosen using the KeywordEditor dialog link, or because
    // the user has cancelled), we pass NULL as the default_search parameter.
    virtual void SearchEngineChosen(const TemplateURL* default_search) = 0;
  };

  // |observer| is the FirstRunView that waits for us to pass back a search
  // engine choice; |profile| allows us to get the set of imported search
  // engines, and display the KeywordEditorView on demand.
  FirstRunSearchEngineView(SearchEngineViewObserver* observer,
                           Profile* profile);
  virtual ~FirstRunSearchEngineView();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);

  // Overridden from views::LinkActivated:
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Overridden from views::WindowDelegate:
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  views::View* GetContentsView() { return this; }
  bool CanResize() const { return false; }
  bool CanMaximize() const { return false; }
  bool IsAlwaysOnTop() const { return false; }
  bool HasAlwaysOnTopMenu() const { return false; }

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from TemplateURLModelObserver. When the search engines have
  // loaded from the profile, we can populate the logos in the dialog box
  // to present to the user.
  virtual void OnTemplateURLModelChanged();

  // Overridden from KeywordEditorViewObserver. If the user chooses to edit
  // search engines from the traditional KeywordEditorView (by clicking the
  // 'search engine options' link in this view), we must wait for that user
  // to finish before completing the import process and launching the browser.
  virtual void OnKeywordEditorClosing(bool default_set);

 private:
  // Initializes the labels and controls in the view.
  void SetupControls();

  // Owned by the profile_.
  TemplateURLModel* search_engines_model_;

  // One for each search engine choice offered, either three or four.
  std::vector<SearchEngineChoice*> search_engine_choices_;

  // The profile associated with this import process.
  Profile* profile_;

  // Gets called back when one of the choice buttons is pressed.
  SearchEngineViewObserver* observer_;

  bool text_direction_is_rtl_;

  // UI elements:
  // Text above the first horizontal separator
  views::Label* title_label_;
  views::Label* text_label_;

  // Horizontal separators
  views::Separator* separator_1_;
  views::Separator* separator_2_;

  // Text below the second horizontal divider. The order of appearance of
  // these three elements is language-dependent.
  views::Label* subtext_label_1_;
  views::Label* subtext_label_2_;
  views::Link* options_link_;

  // Used to figure out positioning of embedded links in RTL languages
  // (see view_text_utils::DrawTextAndPositionUrl).
  views::Label* dummy_subtext_label_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunSearchEngineView);
};

#endif  // CHROME_BROWSER_VIEWS_FIRST_RUN_SEARCH_ENGINE_VIEW_H_

