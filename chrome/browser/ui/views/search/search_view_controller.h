// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEARCH_SEARCH_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SEARCH_SEARCH_VIEW_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/rect.h"

class BrowserView;
class ContentsContainer;
class LocationBarContainer;
class TabContents;

namespace chrome {
namespace search {
class SearchModel;
class SearchViewControllerTest;
class ToolbarSearchAnimator;
}
}

namespace content {
class BrowserContext;
class WebContents;
}

namespace internal {
class SearchNTPContainerView;
class OmniboxPopupContainer;
}

namespace views {
class ImageView;
class Label;
class View;
class WebView;
}

// SearchViewController maintains the search overlay (native new tab page).
// To avoid ordering dependencies this class listens directly to the
// SearchModel of the active tab. BrowserView is responsible for telling this
// class when the active tab changes.
class SearchViewController
    : public chrome::search::SearchModelObserver,
      public ui::ImplicitAnimationObserver,
      public content::NotificationObserver {
  friend class internal::OmniboxPopupContainer;
  friend class chrome::search::SearchViewControllerTest;

 public:
  SearchViewController(
      content::BrowserContext* browser_context,
      ContentsContainer* contents_container,
      chrome::search::ToolbarSearchAnimator* toolbar_search_animator,
      BrowserView* browser_view);
  virtual ~SearchViewController();

  // Set by host to facilitate proper view/layer stacking.
  void set_location_bar_container(
      LocationBarContainer* location_bar_container) {
    location_bar_container_ = location_bar_container;
  }

  // Sets the active tab.
  void SetTabContents(TabContents* tab_contents);

  // Stacks the overlay at the top.
  void StackAtTop();

  // Called when the Instant preview is about to be made the active tab.
  void WillCommitInstant();

  // chrome::search::SearchModelObserver overrides:
  virtual void ModeChanged(const chrome::search::Mode& old_mode,
                           const chrome::search::Mode& new_mode) OVERRIDE;

  // Get the bounds of the omnibox, relative to |destination| when in NTP mode.
  // Returns empty bounds if not in NTP mode.  Coordinates are converted from
  // |ntp_container_| relative to |destination| relative.
  gfx::Rect GetNTPOmniboxBounds(views::View* destination);

  // Get the width and x-position of the omnibox in destination with
  // |destination_width|.
  void GetNTPOmniboxWidthAndXPos(int destination_width,
                                 int* omnibox_width,
                                 int* omnibox_xpos);

 protected:
  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

 private:
  enum State {
    // Search/ntp is not visible.
    STATE_NOT_VISIBLE,

    // Layout for the new tab page prior to web contents section being rendered.
    STATE_NTP_LOADING,

    // Layout for the new tab page.
    STATE_NTP,

    // Animating between STATE_NTP and STATE_SUGGESTIONS.
    STATE_NTP_ANIMATING,

    // Search layout. This is only used when the suggestions UI is visible.
    STATE_SUGGESTIONS,
  };

  // Either NTP is loading or NTP is loaded.
  static bool is_ntp_state(State state);

  // content::NotificationObserver implementation
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Invokes SetState() based on the search model and omnibox.
  void UpdateState();

  // Updates the views and animations. May do any of the following: create the
  // views, start an animation, or destroy the views. What happens is determined
  // from the current state of the SearchModel.
  void SetState(State state);

  // Test support.
  State state() const { return state_; }

  // Starts the animation.
  void StartAnimation();

  // Stops the animation.
  void StopAnimation();

  // Create the various views and installs them as an overlay on
  // |contents_container_|.  |state| is used to determine visual style
  // of the created views.
  void CreateViews(State state);

  // Returns the logo image view, or a name label if an image is not available.
  views::View* GetLogoView() const;

  // Destroys the various views.
  void DestroyViews();

  // Invoked when the visibility of the omnibox popup changes.
  void PopupVisibilityChanged();

  // Access active search model.
  chrome::search::SearchModel* search_model();

  // Access active web contents.
  content::WebContents* web_contents();

  // Test support.
  views::View* ntp_container() const;

  // The profile.  Weak.
  content::BrowserContext* browser_context_;

  // Where the overlay is placed.  Weak.
  ContentsContainer* contents_container_;

  // Weak.
  chrome::search::ToolbarSearchAnimator* toolbar_search_animator_;

  // The browser's main view.  Weak.
  BrowserView* browser_view_;

  // Weak.
  LocationBarContainer* location_bar_container_;

  State state_;

  // The active TabContents.  Weak.  May be NULL.
  TabContents* tab_contents_;

  // The following views are created to render the NTP. Visually they look
  // something like:
  //
  // |---SearchContainerView------------------------------|
  // ||---SearchNTPContainerView & OmniboxPopupViewParent||
  // ||                                                  ||
  // ||     |--Logo or Name------------------------|     ||
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
  // SearchNTPContainerView and OmniboxPopupViewParent are siblings.
  // When on the NTP the OmniboxPopupViewParent is obscured by the
  // SearchNTPContainerView.
  // When on a search page, the SearchNTPContainerView is hidden.

  // The outermost view.  Set as the overlay within |contents_container_| passed
  // in from owning |BrowserView|.
  views::View* search_container_;

  // A container view for the NTP component views.
  internal::SearchNTPContainerView* ntp_container_;

  // The default provider's logo, may be NULL.  When NULL,
  // |default_provider_name_| is used.
  // Lifetime is managed by this controller, not the parent |ntp_container_|.
  scoped_ptr<views::ImageView> default_provider_logo_;

  // The default provider's name. Used as a fallback if the logo is NULL.
  // Lifetime is managed by this controller, not the parent |ntp_container_|.
  scoped_ptr<views::Label> default_provider_name_;

  // An alias to |contents_container_->active()|, but reparented within
  // |ntp_container_| when in the NTP state.
  views::WebView* content_view_;

  // Set to true when theme changes to notify of CSS background y pos regardless
  // if it has changed.
  // Reset to false when this value has been set in |ntp_container_| which
  // actually fires the notification.
  // Theme change always happens in |DEFAULT| mode, when |ntp_container_| is
  // not available, so |SearchViewController| has to listen for theme change,
  // then passes it to |ntp_container_| after it's created for |NTP| mode.
  bool notify_css_background_y_pos_;

  content::NotificationRegistrar registrar_;

  // To send ourselves async messages safely.
  base::WeakPtrFactory<SearchViewController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchViewController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SEARCH_SEARCH_VIEW_CONTROLLER_H_
