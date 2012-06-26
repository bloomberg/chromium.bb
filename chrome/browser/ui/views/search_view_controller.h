// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEARCH_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SEARCH_VIEW_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "ui/compositor/layer_animation_observer.h"

class ContentsContainer;
class LocationBarContainer;
class TabContents;

namespace chrome {
namespace search {
class SearchModel;
}
}

namespace views {
class View;
}

// SearchViewController maintains the search overlay (native new tab page).
// To avoid ordering dependencies this class listens directly to the
// SearchModel of the active tab. BrowserView is responsible for telling this
// class when the active tab changes.
class SearchViewController
    : public chrome::search::SearchModelObserver,
      public ui::ImplicitAnimationObserver {
 public:
  explicit SearchViewController(ContentsContainer* contents_container);
  virtual ~SearchViewController();

  views::View* omnibox_popup_view_parent();

  void set_location_bar_container(
      LocationBarContainer* location_bar_container) {
    location_bar_container_ = location_bar_container;
  }

  // Sets the active tab.
  void SetTabContents(TabContents* tab);

  // Stacks the overlay at the top.
  void StackAtTop();

  // Invoked when the instant preview is ready to be shown.
  void InstantReady();

  // chrome::search::SearchModelObserver overrides:
  virtual void ModeChanged(const chrome::search::Mode& mode) OVERRIDE;

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

 private:
  enum State {
    // Search/ntp is not visible.
    STATE_NOT_VISIBLE,

    // Layout for the new tab page.
    STATE_NTP,

    // Animating between STATE_NTP and STATE_SEARCH.
    STATE_ANIMATING,

    // Search layout. This is only used when the omnibox is visible.
    STATE_SEARCH,
  };

  class OmniboxPopupViewParent;

  // Invokes SetState() based on the search model and omnibox.
  void UpdateState();

  // Updates the views and animations. May do any of the following: create the
  // views, start an animation, or destroy the views. What happens is determined
  // from the current state of the SearchModel.
  void SetState(State state);

  // Starts the animation.
  void StartAnimation();

  // Create the various views and installs them as an overlay on
  // |contents_container_|.
  void CreateViews();

  // Destroys the various views.
  void DestroyViews();

  // Invoked when the visibility of the omnibox popup changes.
  void PopupVisibilityChanged();

  // Returns the SearchModel for the current tab, or NULL if there
  // is no current tab.
  chrome::search::SearchModel* search_model();

  // Where the overlay is placed.
  ContentsContainer* contents_container_;

  LocationBarContainer* location_bar_container_;

  State state_;

  // The active TabContents; may be NULL.
  TabContents* tab_;

  // The following views are created to render the NTP. Visually they look
  // something like:
  //
  // |---SearchContainerView------------------------------|
  // ||-----NTPView & OmniboxPopupViewParent-------------||
  // ||                                                  ||
  // ||     |--LogoView----------------------------|     ||
  // ||     |                                      |     ||
  // ||     |                                      |     ||
  // ||     |--------------------------------------|     ||
  // ||                                                  ||
  // ||        *                                         ||
  // ||                                                  ||
  // ||     |--ContentView-------------------------|     ||
  // ||     |                                      |     ||
  // ||     |                                      |     ||
  // ||     |--------------------------------------|     ||
  // ||                                                  ||
  // ||--------------------------------------------------||
  // |----------------------------------------------------|
  //
  // * - the LocationBarContainer gets positioned here, but it is not a child
  // of any of these views.
  //
  // NTPView and OmniboxPopupViewParent are siblings. When on the NTP the
  // OmniboxPopupViewParent is obscured by the NTPView. When on a search page
  // the NTPView is hidden.
  //
  //
  views::View* search_container_;
  views::View* ntp_view_;
  views::View* logo_view_;
  views::View* content_view_;
  OmniboxPopupViewParent* omnibox_popup_view_parent_;

  DISALLOW_COPY_AND_ASSIGN(SearchViewController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SEARCH_VIEW_CONTROLLER_H_
