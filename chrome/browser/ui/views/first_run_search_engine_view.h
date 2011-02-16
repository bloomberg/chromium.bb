// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIRST_RUN_SEARCH_ENGINE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FIRST_RUN_SEARCH_ENGINE_VIEW_H_
#pragma once

#include <vector>

#include "chrome/browser/search_engines/template_url_model_observer.h"
#include "ui/gfx/size.h"
#include "views/controls/button/native_button.h"
#include "views/view.h"
#include "views/window/window_delegate.h"

namespace views {
class ButtonListener;
class ImageView;
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
class FirstRunSearchEngineView
    : public views::View,
      public views::ButtonListener,
      public views::WindowDelegate,
      public TemplateURLModelObserver {
 public:
  // |profile| allows us to get the set of imported search engines.
  // |randomize| is true if logos are to be displayed in random order.
  FirstRunSearchEngineView(Profile* profile, bool randomize);

  virtual ~FirstRunSearchEngineView();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual AccessibilityTypes::Role GetAccessibleRole();

  // Overridden from views::WindowDelegate:
  virtual std::wstring GetWindowTitle() const;
  views::View* GetContentsView() { return this; }
  bool CanResize() const { return false; }
  bool CanMaximize() const { return false; }
  bool IsAlwaysOnTop() const { return true; }
  bool HasAlwaysOnTopMenu() const { return false; }

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Override from View so we can draw the gray background at dialog top.
  virtual void OnPaint(gfx::Canvas* canvas);

  // Overridden from TemplateURLModelObserver. When the search engines have
  // loaded from the profile, we can populate the logos in the dialog box
  // to present to the user.
  virtual void OnTemplateURLModelChanged();

 private:
  // Initializes the labels and controls in the view.
  void SetupControls();

  // Owned by the profile_.
  TemplateURLModel* search_engines_model_;

  // One for each search engine choice offered, either three or four.
  std::vector<SearchEngineChoice*> search_engine_choices_;

  // If logos are to be displayed in random order. Used for UX testing.
  bool randomize_;

  // The profile associated with this import process.
  Profile* profile_;

  bool text_direction_is_rtl_;

  // Image of browser search box with grey background and bubble arrow.
  views::ImageView* background_image_;

  // UI elements:
  views::Label* title_label_;
  views::Label* text_label_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunSearchEngineView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FIRST_RUN_SEARCH_ENGINE_VIEW_H_

