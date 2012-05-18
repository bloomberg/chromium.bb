// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_INTERSTITIAL_PAGE_IMPL_H_
#define CONTENT_BROWSER_WEB_CONTENTS_INTERSTITIAL_PAGE_IMPL_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/common/renderer_preferences.h"
#include "googleurl/src/gurl.h"

class WebContentsImpl;

namespace content {
class NavigationEntry;
class RenderViewHostImpl;
class WebContentsView;
}

enum ResourceRequestAction {
  BLOCK,
  RESUME,
  CANCEL
};

class CONTENT_EXPORT InterstitialPageImpl
    : public NON_EXPORTED_BASE(content::InterstitialPage),
      public content::NotificationObserver,
      public content::RenderViewHostDelegate,
      public content::RenderWidgetHostDelegate {
 public:
  // The different state of actions the user can take in an interstitial.
  enum ActionState {
    NO_ACTION,           // No action has been taken yet.
    PROCEED_ACTION,      // "Proceed" was selected.
    DONT_PROCEED_ACTION  // "Don't proceed" was selected.
  };

  InterstitialPageImpl(content::WebContents* web_contents,
                       bool new_navigation,
                       const GURL& url,
                       content::InterstitialPageDelegate* delegate);
  virtual ~InterstitialPageImpl();

  // InterstitialPage implementation:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void DontProceed() OVERRIDE;
  virtual void Proceed() OVERRIDE;
  virtual content::RenderViewHost* GetRenderViewHostForTesting() const OVERRIDE;
  virtual content::InterstitialPageDelegate* GetDelegateForTesting() OVERRIDE;
  virtual void DontCreateViewForTesting() OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void Focus() OVERRIDE;

  // Allows the user to navigate away by disabling the interstitial, canceling
  // the pending request, and unblocking the hidden renderer.  The interstitial
  // will stay visible until the navigation completes.
  void CancelForNavigation();

  // Focus the first (last if reverse is true) element in the interstitial page.
  // Called when tab traversing.
  void FocusThroughTabTraversal(bool reverse);

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
  virtual void RenderViewGone(content::RenderViewHost* render_view_host,
                              base::TerminationStatus status,
                              int error_code) OVERRIDE;
  virtual void DidNavigate(
      content::RenderViewHost* render_view_host,
      const ViewHostMsg_FrameNavigate_Params& params) OVERRIDE;
  virtual void UpdateTitle(content::RenderViewHost* render_view_host,
                           int32 page_id,
                           const string16& title,
                           base::i18n::TextDirection title_direction) OVERRIDE;
  virtual content::RendererPreferences GetRendererPrefs(
      content::BrowserContext* browser_context) const OVERRIDE;
  virtual webkit_glue::WebPreferences GetWebkitPrefs() OVERRIDE;
  virtual content::ViewType GetRenderViewType() const OVERRIDE;
  virtual gfx::Rect GetRootWindowResizerRect() const OVERRIDE;

  // RenderWidgetHostDelegate implementation:
  virtual bool PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;

  bool enabled() const { return enabled_; }
  content::WebContents* web_contents() const;
  const GURL& url() const { return url_; }

  // Creates the RenderViewHost containing the interstitial content.
  // Overriden in unit tests.
  virtual content::RenderViewHost* CreateRenderViewHost();

  // Creates the WebContentsView that shows the interstitial RVH.
  // Overriden in unit tests.
  virtual content::WebContentsView* CreateWebContentsView();

  // Notification magic.
  content::NotificationRegistrar notification_registrar_;

 private:
  class InterstitialPageRVHViewDelegate;

  // Disable the interstitial:
  // - if it is not yet showing, then it won't be shown.
  // - any command sent by the RenderViewHost will be ignored.
  void Disable();

  // Executes the passed action on the ResourceDispatcher (on the IO thread).
  // Used to block/resume/cancel requests for the RenderViewHost hidden by this
  // interstitial.
  void TakeActionOnResourceDispatcher(ResourceRequestAction action);

  // The contents in which we are displayed.
  WebContentsImpl* web_contents_;

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
  content::RenderViewHostImpl* render_view_host_;

  // The IDs for the Render[View|Process]Host hidden by this interstitial.
  int original_child_id_;
  int original_rvh_id_;

  // Whether or not we should change the title of the contents when hidden (to
  // revert it to its original value).
  bool should_revert_web_contents_title_;

  // Whether or not the contents was loading resources when the interstitial was
  // shown.  We restore this state if the user proceeds from the interstitial.
  bool web_contents_was_loading_;

  // Whether the ResourceDispatcherHost has been notified to cancel/resume the
  // resource requests blocked for the RenderViewHost.
  bool resource_dispatcher_host_notified_;

  // The original title of the contents that should be reverted to when the
  // interstitial is hidden.
  string16 original_web_contents_title_;

  // Our RenderViewHostViewDelegate, necessary for accelerators to work.
  scoped_ptr<InterstitialPageRVHViewDelegate> rvh_view_delegate_;

  // Settings passed to the renderer.
  mutable content::RendererPreferences renderer_preferences_;

  bool create_view_;

  scoped_ptr<content::InterstitialPageDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialPageImpl);
};

#endif  // CONTENT_BROWSER_WEB_CONTENTS_INTERSTITIAL_PAGE_IMPL_H_
