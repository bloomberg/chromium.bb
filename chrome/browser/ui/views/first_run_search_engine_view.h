// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIRST_RUN_SEARCH_ENGINE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FIRST_RUN_SEARCH_ENGINE_VIEW_H_
#pragma once

#include <vector>

#include "chrome/browser/search_engines/template_url_service_observer.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/widget/widget_delegate.h"
#include "views/view.h"

class Profile;
class TemplateURL;
class TemplateURLService;
class ThemeService;

namespace views {
class ImageView;
class Label;
}

// This class holds the logo and TemplateURL for a search engine and serves
// as its button in the search engine selection view.
class SearchEngineChoice : public views::NativeTextButton {
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
  views::View* GetView() { return choice_view_; }
  int GetChoiceViewWidth();
  int GetChoiceViewHeight();

  // Set the bounds for the search engine choice view; called in the
  // Layout method, when we know what the new bounds should be.
  void SetChoiceViewBounds(int x, int y, int width, int height);

  // Accessor for the search engine data this button represents.
  const TemplateURL* GetSearchEngine() { return search_engine_; }

  // Used for UX testing.
  void set_slot(int slot) { slot_ = slot; }
  int slot() const { return slot_; }

 private:
  // Either an ImageView of a logo, or a Label with text.  Owned by
  // FirstRunSearchEngineView.
  views::View* choice_view_;

  // True if choice_view_ is holding an ImageView.
  bool is_image_label_;

  // Data for the search engine held here.
  const TemplateURL* search_engine_;

  // Used for UX testing. Gives slot in which search engine was shown.
  int slot_;

  DISALLOW_COPY_AND_ASSIGN(SearchEngineChoice);
};

// This class displays a large search engine choice dialog view during
// initial first run import.
class FirstRunSearchEngineView : public views::WidgetDelegateView,
                                 public views::ButtonListener,
                                 public TemplateURLServiceObserver {
 public:
  // |profile| allows us to get the set of imported search engines.
  // |randomize| is true if logos are to be displayed in random order.
  FirstRunSearchEngineView(Profile* profile, bool randomize);

  virtual ~FirstRunSearchEngineView();

  // Overridden from views::WidgetDelegateView:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE { return this; }
  virtual void WindowClosing() OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // Override from views::View so we can draw the gray background at dialog top.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from TemplateURLServiceObserver. When the search engines have
  // loaded from the profile, we can populate the logos in the dialog box
  // to present to the user.
  virtual void OnTemplateURLServiceChanged() OVERRIDE;

#if defined(UNIT_TEST)
  void set_quit_on_closing(bool quit_on_closing) {
    quit_on_closing_ = quit_on_closing;
  }
#endif

 private:
  // Once the TemplateURLService has loaded and we're in a View hierarchy, it's
  // OK to add the search engines from the TemplateURLService.
  void AddSearchEnginesIfPossible();

  // Sets the default search engine to the one represented by |choice|.
  void ChooseSearchEngine(SearchEngineChoice* choice);

  // One for each search engine choice offered, either three or four.
  std::vector<SearchEngineChoice*> search_engine_choices_;

  // If logos are to be displayed in random order. Used for UX testing.
  bool randomize_;

  // Services associated with the current profile.
  TemplateURLService* template_url_service_;
  ThemeService* theme_service_;

  bool text_direction_is_rtl_;

  bool added_to_view_hierarchy_;

  // Image of browser search box with grey background and bubble arrow.
  views::ImageView* background_image_;

  // UI elements:
  views::Label* title_label_;
  views::Label* text_label_;

  // True when the user has chosen a particular search engine. Defaults to
  // false. When the user closes the window without choosing a search engine,
  // the engine specified by |fallback_choice_| is chosen.
  bool user_chosen_engine_;

  // The engine to choose when the user closes the window without explicitly
  // making a selection. Because of randomization functionality, we cannot
  // reliably deduce this from slot order, so this value is saved prior to
  // randomization.
  SearchEngineChoice* fallback_choice_;

  // Defaults to true. Indicates that the current message loop should be quit
  // when the window is closed. This is false in tests when this dialog does not
  // spin its own message loop.
  bool quit_on_closing_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunSearchEngineView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FIRST_RUN_SEARCH_ENGINE_VIEW_H_
