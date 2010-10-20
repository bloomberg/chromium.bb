// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_LOADER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_LOADER_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/browser/instant/instant_commit_type.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "chrome/common/page_transition_types.h"
#include "gfx/rect.h"
#include "googleurl/src/gurl.h"

class InstantLoaderDelegate;
class InstantLoaderManagerTest;
class TabContents;
class TemplateURL;

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
class InstantLoader {
 public:
  InstantLoader(InstantLoaderDelegate* delegate, TemplateURLID id);
  ~InstantLoader();

  // Invoked to load a URL. |tab_contents| is the TabContents the preview is
  // going to be shown on top of and potentially replace.
  void Update(TabContents* tab_contents,
              const TemplateURL* template_url,
              const GURL& url,
              PageTransition::Type transition_type,
              const string16& user_text,
              string16* suggested_text);

  // Sets the bounds of the omnibox (in screen coordinates). The bounds are
  // remembered until the preview is committed or destroyed. This is only used
  // when showing results for a search provider that supports instant.
  void SetOmniboxBounds(const gfx::Rect& bounds);

  // Returns true if the mouse is down as the result of activating the preview
  // content.
  bool IsMouseDownFromActivate();

  // Releases the preview TabContents passing ownership to the caller. This is
  // intended to be called when the preview TabContents is committed. This does
  // not notify the delegate.
  TabContents* ReleasePreviewContents(InstantCommitType type);

  // Calls through to method of same name on delegate.
  bool ShouldCommitInstantOnMouseUp();
  void CommitInstantLoader();

  // Resets the template_url_id_ to zero and shows this loader. This is only
  // intended to be invoked from InstantLoaderDoesntSupportInstant.
  void ClearTemplateURLID();

  // The preview TabContents; may be null.
  TabContents* preview_contents() const { return preview_contents_.get(); }

  // Returns true if the preview TabContents is ready to be shown.
  bool ready() const { return ready_; }

  const GURL& url() const { return url_; }

  // Are we showing instant results?
  bool is_showing_instant() const { return template_url_id_ != 0; }

  // If we're showing instant this returns non-zero.
  TemplateURLID template_url_id() const { return template_url_id_; }

  // See description above field.
  const string16& user_text() const { return user_text_; }

 private:
  friend class InstantLoaderManagerTest;
  class FrameLoadObserver;
  class PaintObserverImpl;
  class TabContentsDelegateImpl;

  // Invoked when the page wants to update the suggested text. If |user_text_|
  // starts with |suggested_text|, then the delegate is notified of the change,
  // which results in updating the omnibox.
  void SetCompleteSuggestedText(const string16& suggested_text);

  // Invoked when the page paints.
  void PreviewPainted();

  // Invoked to show the preview. This is invoked in two possible cases: when
  // the renderer paints, or when an auth dialog is shown. This notifies the
  // delegate the preview is ready to be shown.
  void ShowPreview();

  // Invoked once the page has finished loading and the script has been sent.
  void PageFinishedLoading();

  // Returns the bounds of the omnibox in terms of the preview tab contents.
  gfx::Rect GetOmniboxBoundsInTermsOfPreview();

  // Are we waiting for the preview page to finish loading?
  bool is_waiting_for_load() const {
    return frame_load_observer_.get() != NULL;
  }

  // Invoked if it the page doesn't really support instant when we thought it
  // did. If |needs_reload| is true, the text changed since the first load and
  // the page needs to be reloaded.
  void PageDoesntSupportInstant(bool needs_reload);

  // Invoked from the timer to update the bounds of the omnibox.
  void ProcessBoundsChange();

  InstantLoaderDelegate* delegate_;

  // If we're showing instant results this is the ID of the TemplateURL driving
  // the results. A value of 0 means there is no TemplateURL.
  TemplateURLID template_url_id_;

  // The url we're displaying.
  GURL url_;

  // The URL first used to load instant results.
  GURL initial_instant_url_;

  // Delegate of the preview TabContents. Used to detect when the user does some
  // gesture on the TabContents and the preview needs to be activated.
  scoped_ptr<TabContentsDelegateImpl> preview_tab_contents_delegate_;

  // The preview TabContents; may be null.
  scoped_ptr<TabContents> preview_contents_;

  // Is the preview_contents ready to be shown?
  bool ready_;

  // The text the user typed in the omnibox.
  string16 user_text_;

  // The latest suggestion from the page.
  string16 complete_suggested_text_;

  // See description above setter.
  gfx::Rect omnibox_bounds_;

  // Last bounds passed to the page.
  gfx::Rect last_omnibox_bounds_;

  scoped_ptr<FrameLoadObserver> frame_load_observer_;

  // Transition type of the match last passed to Update.
  PageTransition::Type last_transition_type_;

  // Timer used to update the bounds of the omnibox.
  base::OneShotTimer<InstantLoader> update_bounds_timer_;

  DISALLOW_COPY_AND_ASSIGN(InstantLoader);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_LOADER_H_
