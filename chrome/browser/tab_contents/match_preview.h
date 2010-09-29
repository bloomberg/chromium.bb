// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_MATCH_PREVIEW_H_
#define CHROME_BROWSER_TAB_CONTENTS_MATCH_PREVIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "chrome/common/page_transition_types.h"
#include "gfx/rect.h"
#include "googleurl/src/gurl.h"

struct AutocompleteMatch;
class MatchPreviewDelegate;
class TabContents;

// MatchPreview maintains a TabContents that is intended to give a preview of
// a URL. MatchPreview is owned by Browser.
//
// At any time the TabContents maintained by MatchPreview may be destroyed by
// way of |DestroyPreviewContents|, which results in |HideMatchPreview| being
// invoked on the delegate. Similarly the preview may be committed at any time
// by invoking |CommitCurrentPreview|, which results in |CommitMatchPreview|
// being invoked on the delegate.
class MatchPreview {
 public:
  enum CommitType {
    // The commit is the result of the user pressing enter.
    COMMIT_PRESSED_ENTER,

    // The commit is the result of focus being lost. This typically corresponds
    // to a mouse click event.
    COMMIT_FOCUS_LOST,

    // Used internally.
    COMMIT_DESTROY
  };

  explicit MatchPreview(MatchPreviewDelegate* delegate);
  ~MatchPreview();

  // Is MatchPreview enabled?
  static bool IsEnabled();

  // Invoked as the user types in the omnibox with the url to navigate to.  If
  // the url is empty and there is a preview TabContents it is destroyed. If url
  // is non-empty and the preview TabContents has not been created it is
  // created.
  void Update(TabContents* tab_contents,
              const AutocompleteMatch& match,
              const string16& user_text,
              string16* suggested_text);

  // Sets the bounds of the omnibox (in screen coordinates). The bounds are
  // remembered until the preview is committed or destroyed. This is only used
  // when showing results for a search provider that supports instant.
  void SetOmniboxBounds(const gfx::Rect& bounds);

  // Destroys the preview TabContents. Does nothing if the preview TabContents
  // has not been created.
  void DestroyPreviewContents();

  // Invoked when the user does some gesture that should trigger making the
  // current previewed page the permanent page.
  void CommitCurrentPreview(CommitType type);

  // Sets MatchPreview so that when the mouse is released the preview is
  // committed.
  void SetCommitOnMouseUp();

  // Returns true if the preview will be committed on mouse up.
  bool commit_on_mouse_up() const { return commit_on_mouse_up_; }

  // Returns true if the mouse is down as the result of activating the preview
  // content.
  bool IsMouseDownFromActivate();

  // Releases the preview TabContents passing ownership to the caller. This is
  // intended to be called when the preview TabContents is committed. This does
  // not notify the delegate.
  TabContents* ReleasePreviewContents(CommitType type);

  // TabContents the match is being shown for.
  TabContents* tab_contents() const { return tab_contents_; }

  // The preview TabContents; may be null.
  TabContents* preview_contents() const { return preview_contents_.get(); }

  // Returns true if the preview TabContents is active. In some situations this
  // may return false yet preview_contents() returns non-NULL.
  bool is_active() const { return is_active_; }

  // Returns the transition type of the last AutocompleteMatch passed to Update.
  PageTransition::Type last_transition_type() const {
    return last_transition_type_;
  }

  const GURL& url() const { return url_; }

  // Are we showing instant results?
  bool is_showing_instant() const { return template_url_id_ != 0; }

 private:
  class FrameLoadObserver;
  class PaintObserverImpl;
  class TabContentsDelegateImpl;

  // Invoked when the page wants to update the suggested text. If |user_text_|
  // starts with |suggested_text|, then the delegate is notified of the change,
  // which results in updating the omnibox.
  void SetCompleteSuggestedText(const string16& suggested_text);

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

  MatchPreviewDelegate* delegate_;

  // The TabContents last passed to |Update|.
  TabContents* tab_contents_;

  // The url we're displaying.
  GURL url_;

  // Delegate of the preview TabContents. Used to detect when the user does some
  // gesture on the TabContents and the preview needs to be activated.
  scoped_ptr<TabContentsDelegateImpl> preview_tab_contents_delegate_;

  // The preview TabContents; may be null.
  scoped_ptr<TabContents> preview_contents_;

  // Has notification been sent out that the preview TabContents is ready to be
  // shown?
  bool is_active_;

  // The text the user typed in the omnibox.
  string16 user_text_;

  // The latest suggestion from the page.
  string16 complete_suggested_text_;

  // If we're showing instant results this is the ID of the TemplateURL driving
  // the results. A value of 0 means there is no TemplateURL.
  TemplateURLID template_url_id_;

  // See description above setter.
  gfx::Rect omnibox_bounds_;

  scoped_ptr<FrameLoadObserver> frame_load_observer_;

  // See description above CommitOnMouseUp.
  bool commit_on_mouse_up_;

  // See description above getter.
  PageTransition::Type last_transition_type_;

  DISALLOW_COPY_AND_ASSIGN(MatchPreview);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_MATCH_PREVIEW_H_
