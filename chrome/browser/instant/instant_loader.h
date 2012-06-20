// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_LOADER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_LOADER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/browser/instant/instant_commit_type.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "chrome/common/instant_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"

class InstantLoaderDelegate;
class InstantLoaderManagerTest;
class TabContents;
class TemplateURL;

namespace content {
class SessionStorageNamespace;
}

// InstantLoader does the loading of a particular URL for InstantController.
// InstantLoader notifies its delegate, which is typically InstantController, of
// all interesting events.
//
// InstantLoader is created with a TemplateURLID. If non-zero InstantLoader
// first determines if the site actually supports instant. If it doesn't, the
// delegate is notified by way of |InstantLoaderDoesntSupportInstant|.
//
// If the TemplateURLID supplied to the constructor is zero, then the url is
// loaded as is.
class InstantLoader : public content::NotificationObserver {
 public:
  // Header and value set on loads that originate from instant.
  static const char* const kInstantHeader;
  static const char* const kInstantHeaderValue;

  // |group| is an identifier suffixed to histograms to distinguish field trial
  // statistics from regular operation; can be a blank string.
  InstantLoader(InstantLoaderDelegate* delegate,
                TemplateURLID id,
                const std::string& group);
  virtual ~InstantLoader();

  // Invoked to load a URL. |tab_contents| is the TabContents the preview
  // is going to be shown on top of and potentially replace. Returns true if the
  // arguments differ from the last call to |Update|.
  bool Update(TabContents* tab_contents,
              const TemplateURL* template_url,
              const GURL& url,
              content::PageTransition transition_type,
              const string16& user_text,
              bool verbatim,
              string16* suggested_text);

  // Sets the bounds of the omnibox (in screen coordinates). The bounds are
  // remembered until the preview is committed or destroyed. This is only used
  // when showing results for a search provider that supports instant.
  void SetOmniboxBounds(const gfx::Rect& bounds);

  // Returns true if the mouse is down as the result of activating the preview
  // content.
  bool IsMouseDownFromActivate();

  // Releases the preview TabContents passing ownership to the caller.
  // This is intended to be called when the preview TabContents is
  // committed. This does not notify the delegate. |tab_contents| is the
  // underlying tab onto which the preview will be committed. It can be NULL
  // when the underlying tab is irrelevant, for example when |type| is
  // INSTANT_COMMIT_DESTROY.
  TabContents* ReleasePreviewContents(InstantCommitType type,
                                      TabContents* tab_contents);

  // Calls through to method of same name on delegate.
  bool ShouldCommitInstantOnMouseUp();
  void CommitInstantLoader();

  // Preload |template_url|'s instant URL, if the loader doesn't already have
  // a |preview_contents()| for it.
  void MaybeLoadInstantURL(TabContents* tab_contents,
                           const TemplateURL* template_url);

  // Returns true if the preview NavigationController's WebContents has a
  // pending NavigationEntry.
  bool IsNavigationPending() const;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // The preview TabContents; may be null.
  TabContents* preview_contents() const {
    return preview_contents_.get();
  }

  // Returns true if the preview TabContents is ready to be shown. A
  // non-instant loader is ready once the renderer paints, otherwise it isn't
  // ready until we get a response back from the page.
  bool ready() const { return ready_; }

  // Returns true if the current load returned a 200.
  bool http_status_ok() const { return http_status_ok_; }

  // Returns true if the url needs to be reloaded. This is set to true for
  // downloads.
  bool needs_reload() const { return needs_reload_; }

  const GURL& url() const { return url_; }

  bool verbatim() const { return verbatim_; }

  // Are we showing instant results?
  bool is_showing_instant() const { return template_url_id_ != 0; }

  // If we're showing instant this returns non-zero.
  TemplateURLID template_url_id() const { return template_url_id_; }

  // See description above field.
  const string16& user_text() const { return user_text_; }

  // Are we waiting for the preview page to finish loading and to determine if
  // it supports instant?
  bool is_determining_if_page_supports_instant() const {
    return frame_load_observer_.get() != NULL;
  }

 private:
  friend class InstantLoaderManagerTest;
  friend class InstantTest;
  class FrameLoadObserver;
  class PaintObserverImpl;
  class TabContentsDelegateImpl;

  // Invoked when the page wants to update the suggested text. If |user_text_|
  // starts with |suggested_text|, then the delegate is notified of the change,
  // which results in updating the omnibox.
  void SetCompleteSuggestedText(const string16& suggested_text,
                                InstantCompleteBehavior behavior);

  // Invoked when the page paints.
  void PreviewPainted();

  // Invoked when the http status code changes. This may notify the delegate.
  void SetHTTPStatusOK(bool is_ok);

  // Invoked to show the preview. This is invoked in two possible cases: when
  // the renderer paints, or when an auth dialog is shown. This notifies the
  // delegate the preview is ready to be shown.
  void ShowPreview();

  // Invoked once the page has finished loading and the script has been sent.
  void PageFinishedLoading();

  // Returns the bounds of the omnibox in terms of the preview tab contents.
  gfx::Rect GetOmniboxBoundsInTermsOfPreview();

  // Invoked if it the page doesn't really support instant when we thought it
  // did. If |needs_reload| is true, the text changed since the first load and
  // the page needs to be reloaded.
  void PageDoesntSupportInstant(bool needs_reload);

  // Invokes |SetBoundsToPage(false)|. This is called from the timer.
  void ProcessBoundsChange();

  // Notifes the page of the omnibox bounds. If |force_if_loading| is true the
  // bounds are sent down even if we're waiting on the load, otherwise if we're
  // waiting on the load and |force_if_loading| is false this does nothing.
  void SendBoundsToPage(bool force_if_loading);

  // Called when the TabContentsDelegate wants to swap a new TabContents
  // into our |preview_contents_|.
  void ReplacePreviewContents(TabContents* old_tc,
                              TabContents* new_tc);

  // Called to set up the |preview_contents_| based on |tab_contents| when it is
  // created or replaced.
  void SetupPreviewContents(TabContents* tab_contents);

  // Creates and sets the preview TabContents.
  void CreatePreviewContents(TabContents* tab_contents);

  // Creates and loads the |template_url|'s instant URL.
  void LoadInstantURL(const TemplateURL* template_url,
                      content::PageTransition transition_type,
                      const string16& user_text,
                      bool verbatim,
                      bool override_user_agent);

  InstantLoaderDelegate* delegate_;

  // If we're showing instant results this is the ID of the TemplateURL driving
  // the results. A value of 0 means there is no TemplateURL.
  const TemplateURLID template_url_id_;

  // The url we're displaying.
  GURL url_;

  // Delegate of the preview WebContents. Used to detect when the user does some
  // gesture on the WebContents and the preview needs to be activated.
  scoped_ptr<TabContentsDelegateImpl> preview_tab_contents_delegate_;

  // The preview TabContents; may be null.
  scoped_ptr<TabContents> preview_contents_;

  // Is the preview_contents ready to be shown?
  bool ready_;

  // Was the last status code a 200?
  bool http_status_ok_;

  // The text the user typed in the omnibox, stripped of the leading ?, if any.
  string16 user_text_;

  // The latest suggestion from the page.
  string16 complete_suggested_text_;

  // The latest suggestion (suggested text less the user text).
  string16 last_suggestion_;

  // See description above setter.
  gfx::Rect omnibox_bounds_;

  // Last bounds passed to the page.
  gfx::Rect last_omnibox_bounds_;

  scoped_ptr<FrameLoadObserver> frame_load_observer_;

  // Transition type of the match last passed to Update.
  content::PageTransition last_transition_type_;

  // Timer used to update the bounds of the omnibox.
  base::OneShotTimer<InstantLoader> update_bounds_timer_;

  // Used to get notifications about renderers coming and going.
  content::NotificationRegistrar registrar_;

  // Last value of verbatim passed to |Update|.
  bool verbatim_;

  // True if the page needs to be reloaded.
  bool needs_reload_;

  // See description above constructor.
  std::string group_;

  // The session storage namespace identifier of the original tab contents that
  // the preview_contents_ was based upon.
  scoped_refptr<content::SessionStorageNamespace> session_storage_namespace_;

  DISALLOW_COPY_AND_ASSIGN(InstantLoader);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_LOADER_H_
