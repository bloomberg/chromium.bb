// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_INTERSTITIAL_PAGE_H_
#define CONTENT_BROWSER_TAB_CONTENTS_INTERSTITIAL_PAGE_H_
#pragma once

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/renderer_preferences.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/size.h"

class NavigationEntry;
class TabContents;
class TabContentsView;

// This class is a base class for interstitial pages, pages that show some
// informative message asking for user validation before reaching the target
// page. (Navigating to a page served over bad HTTPS or a page containing
// malware are typical cases where an interstitial is required.)
//
// If specified in its constructor, this class creates a navigation entry so
// that when the interstitial shows, the current entry is the target URL.
//
// InterstitialPage instances take care of deleting themselves when closed
// through a navigation, the TabContents closing them or the tab containing them
// being closed.

enum ResourceRequestAction {
  BLOCK,
  RESUME,
  CANCEL
};

class CONTENT_EXPORT InterstitialPage : public content::NotificationObserver,
                                        public RenderViewHostDelegate {
 public:
  // The different state of actions the user can take in an interstitial.
  enum ActionState {
    NO_ACTION,           // No action has been taken yet.
    PROCEED_ACTION,      // "Proceed" was selected.
    DONT_PROCEED_ACTION  // "Don't proceed" was selected.
  };

  // Creates an interstitial page to show in |tab|. |new_navigation| should be
  // set to true when the interstitial is caused by loading a new page, in which
  // case a temporary navigation entry is created with the URL |url| and
  // added to the navigation controller (so the interstitial page appears as a
  // new navigation entry). |new_navigation| should be false when the
  // interstitial was triggered by a loading a sub-resource in a page.
  InterstitialPage(TabContents* tab, bool new_navigation, const GURL& url);
  virtual ~InterstitialPage();

  // Shows the interstitial page in the tab.
  virtual void Show();

  // Hides the interstitial page. Warning: this deletes the InterstitialPage.
  void Hide();

  // Retrieves the InterstitialPage if any associated with the specified
  // |tab_contents| (used by ui tests).
  static InterstitialPage* GetInterstitialPage(TabContents* tab_contents);

  // Sub-classes should return the HTML that should be displayed in the page.
  virtual std::string GetHTMLContents();

  // Reverts to the page showing before the interstitial.
  // Sub-classes should call this method when the user has chosen NOT to proceed
  // to the target URL.
  // Warning: if |new_navigation| was set to true in the constructor, 'this'
  //          will be deleted when this method returns.
  virtual void DontProceed();

  // Sub-classes should call this method when the user has chosen to proceed to
  // the target URL.
  // Warning: 'this' has been deleted when this method returns.
  virtual void Proceed();

  // Allows the user to navigate away by disabling the interstitial, canceling
  // the pending request, and unblocking the hidden renderer.  The interstitial
  // will stay visible until the navigation completes.
  void CancelForNavigation();

  // Sizes the RenderViewHost showing the actual interstitial page contents.
  void SetSize(const gfx::Size& size);

  ActionState action_taken() const { return action_taken_; }

  // Sets the focus to the interstitial.
  void Focus();

  // Focus the first (last if reverse is true) element in the interstitial page.
  // Called when tab traversing.
  void FocusThroughTabTraversal(bool reverse);

  virtual content::ViewType GetRenderViewType() const OVERRIDE;

  // See description above field.
  void set_reload_on_dont_proceed(bool value) {
    reload_on_dont_proceed_ = value;
  }
  bool reload_on_dont_proceed() const { return reload_on_dont_proceed_; }

 protected:
  // content::NotificationObserver method:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // RenderViewHostDelegate implementation:
  virtual View* GetViewDelegate() OVERRIDE;
  virtual const GURL& GetURL() const OVERRIDE;
  virtual void RenderViewGone(RenderViewHost* render_view_host,
                              base::TerminationStatus status,
                              int error_code) OVERRIDE;
  virtual void DidNavigate(
      RenderViewHost* render_view_host,
      const ViewHostMsg_FrameNavigate_Params& params) OVERRIDE;
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id,
                           const string16& title,
                           base::i18n::TextDirection title_direction) OVERRIDE;
  virtual content::RendererPreferences GetRendererPrefs(
      content::BrowserContext* browser_context) const OVERRIDE;

  // Invoked with the NavigationEntry that is going to be added to the
  // navigation controller.
  // Gives an opportunity to sub-classes to set states on the |entry|.
  // Note that this is only called if the InterstitialPage was constructed with
  // |create_navigation_entry| set to true.
  virtual void UpdateEntry(NavigationEntry* entry) {}

  bool enabled() const { return enabled_; }
  TabContents* tab() const { return tab_; }
  const GURL& url() const { return url_; }
  RenderViewHost* render_view_host() const { return render_view_host_; }
  void set_renderer_preferences(const content::RendererPreferences& prefs) {
    renderer_preferences_ = prefs;
  }

  // Creates the RenderViewHost containing the interstitial content.
  // Overriden in unit tests.
  virtual RenderViewHost* CreateRenderViewHost();

  // Creates the TabContentsView that shows the interstitial RVH.
  // Overriden in unit tests.
  virtual TabContentsView* CreateTabContentsView();

  // Notification magic.
  content::NotificationRegistrar notification_registrar_;

 private:
  // AutomationProvider needs access to Proceed and DontProceed to simulate
  // user actions.
  friend class AutomationProvider;

  class InterstitialPageRVHViewDelegate;

  // Initializes tab_to_interstitial_page_ in a thread-safe manner.
  // Should be called before accessing tab_to_interstitial_page_.
  static void InitInterstitialPageMap();

  // Disable the interstitial:
  // - if it is not yet showing, then it won't be shown.
  // - any command sent by the RenderViewHost will be ignored.
  void Disable();

  // Executes the passed action on the ResourceDispatcher (on the IO thread).
  // Used to block/resume/cancel requests for the RenderViewHost hidden by this
  // interstitial.
  void TakeActionOnResourceDispatcher(ResourceRequestAction action);

  // The tab in which we are displayed.
  TabContents* tab_;

  // The URL that is shown when the interstitial is showing.
  GURL url_;

  // Whether this interstitial is shown as a result of a new navigation (in
  // which case a transient navigation entry is created).
  bool new_navigation_;

  // Whether we should discard the pending navigation entry when not proceeding.
  // This is to deal with cases where |new_navigation_| is true but a new
  // pending entry was created since this interstitial was shown and we should
  // not discard it.
  bool should_discard_pending_nav_entry_;

  // If true and the user chooses not to proceed the target NavigationController
  // is reloaded. This is used when two NavigationControllers are merged
  // (CopyStateFromAndPrune).
  // The default is false.
  bool reload_on_dont_proceed_;

  // Whether this interstitial is enabled.  See Disable() for more info.
  bool enabled_;

  // Whether the Proceed or DontProceed methods have been called yet.
  ActionState action_taken_;

  // The RenderViewHost displaying the interstitial contents.
  RenderViewHost* render_view_host_;

  // The IDs for the Render[View|Process]Host hidden by this interstitial.
  int original_child_id_;
  int original_rvh_id_;

  // Whether or not we should change the title of the tab when hidden (to revert
  // it to its original value).
  bool should_revert_tab_title_;

  // Whether or not the tab was loading resources when the interstitial was
  // shown.  We restore this state if the user proceeds from the interstitial.
  bool tab_was_loading_;

  // Whether the ResourceDispatcherHost has been notified to cancel/resume the
  // resource requests blocked for the RenderViewHost.
  bool resource_dispatcher_host_notified_;

  // The original title of the tab that should be reverted to when the
  // interstitial is hidden.
  string16 original_tab_title_;

  // Our RenderViewHostViewDelegate, necessary for accelerators to work.
  scoped_ptr<InterstitialPageRVHViewDelegate> rvh_view_delegate_;

  // We keep a map of the various blocking pages shown as the UI tests need to
  // be able to retrieve them.
  typedef std::map<TabContents*, InterstitialPage*> InterstitialPageMap;
  static InterstitialPageMap* tab_to_interstitial_page_;

  // Settings passed to the renderer.
  content::RendererPreferences renderer_preferences_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialPage);
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_INTERSTITIAL_PAGE_H_
