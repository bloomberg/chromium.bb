// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
#pragma once

#include <set>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/browser/instant/instant_commit_type.h"
#include "chrome/browser/instant/instant_loader_delegate.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "chrome/common/page_transition_types.h"
#include "gfx/native_widget_types.h"
#include "gfx/rect.h"
#include "googleurl/src/gurl.h"

struct AutocompleteMatch;
class InstantDelegate;
class InstantLoaderManager;
class PrefService;
class Profile;
class TabContents;
class TemplateURL;

// InstantController maintains a TabContents that is intended to give a preview
// of a URL. InstantController is owned by Browser.
//
// At any time the TabContents maintained by InstantController may be destroyed
// by way of |DestroyPreviewContents|, which results in |HideInstant| being
// invoked on the delegate. Similarly the preview may be committed at any time
// by invoking |CommitCurrentPreview|, which results in |CommitInstant|
// being invoked on the delegate.
class InstantController : public InstantLoaderDelegate {
 public:
  // Variations of instant support.
  enum Type {
    // NOTE: these values are persisted to prefs. Don't change them!

    // Search results are shown for the best guess of what we think the user was
    // planning on typing.
    PREDICTIVE_TYPE = 0,

    // Search results are shown for exactly what was typed.
    VERBATIM_TYPE,

    LAST_TYPE = VERBATIM_TYPE,
  };

  InstantController(Profile* profile, InstantDelegate* delegate);
  ~InstantController();

  // Registers instant related preferences.
  static void RegisterUserPrefs(PrefService* prefs);

  // Records instant metrics.
  static void RecordMetrics(Profile* profile);

  // Returns true if either type of instant is enabled.
  static bool IsEnabled(Profile* profile);

  // Returns true if the specified type of instant is enabled.
  static bool IsEnabled(Profile* profile, Type type);

  // Enables instant.
  static void Enable(Profile* profile);

  // Disables instant.
  static void Disable(Profile* profile);

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

  // Returns true if we're showing the last URL passed to |Update|. If this is
  // false a commit does not result in committing the last url passed to update.
  // A return value of false happens if we're in the process of determining if
  // the page supports instant.
  bool IsCurrent();

  // Invoked when the user does some gesture that should trigger making the
  // current previewed page the permanent page.
  void CommitCurrentPreview(InstantCommitType type);

  // Sets InstantController so that when the mouse is released the preview is
  // committed.
  void SetCommitOnMouseUp();

  bool commit_on_mouse_up() const { return commit_on_mouse_up_; }

  // Returns true if the mouse is down as the result of activating the preview
  // content.
  bool IsMouseDownFromActivate();

  // The autocomplete edit that was initiating the current instant session has
  // lost focus. Commit or discard the preview accordingly.
  void OnAutocompleteLostFocus(gfx::NativeView view_gaining_focus);

  // Releases the preview TabContents passing ownership to the caller. This is
  // intended to be called when the preview TabContents is committed. This does
  // not notify the delegate.
  // WARNING: be sure and invoke CompleteRelease after adding the returned
  // TabContents to a tabstrip.
  TabContents* ReleasePreviewContents(InstantCommitType type);

  // Does cleanup after the preview contents has been added to the tabstrip.
  // Invoke this if you explicitly invoke ReleasePreviewContents.
  void CompleteRelease(TabContents* tab);

  // TabContents the match is being shown for.
  TabContents* tab_contents() const { return tab_contents_; }

  // The preview TabContents; may be null.
  TabContents* GetPreviewContents();

  // Returns true if the preview TabContents is active. In some situations this
  // may return false yet preview_contents() returns non-NULL.
  bool is_active() const { return is_active_; }

  // Returns the transition type of the last AutocompleteMatch passed to Update.
  PageTransition::Type last_transition_type() const {
    return last_transition_type_;
  }

  // Are we showing instant results?
  bool IsShowingInstant();

  // InstantLoaderDelegate
  virtual void ShowInstantLoader(InstantLoader* loader);
  virtual void SetSuggestedTextFor(InstantLoader* loader,
                                   const string16& text);
  virtual gfx::Rect GetInstantBounds();
  virtual bool ShouldCommitInstantOnMouseUp();
  virtual void CommitInstantLoader(InstantLoader* loader);
  virtual void InstantLoaderDoesntSupportInstant(InstantLoader* loader,
                                                 bool needs_reload,
                                                 const GURL& url_to_load);

 private:
  // Returns true if we should update immediately.
  bool ShouldUpdateNow(TemplateURLID instant_id, const GURL& url);

  // Schedules a delayed update to load the specified url.
  void ScheduleUpdate(const GURL& url);

  // Invoked from the timer to process the last scheduled url.
  void ProcessScheduledUpdate();

  // Updates InstantLoaderManager and its current InstantLoader. This is invoked
  // internally from Update.
  void UpdateLoader(const TemplateURL* template_url,
                    const GURL& url,
                    PageTransition::Type transition_type,
                    const string16& user_text,
                    string16* suggested_text);

  // Returns true if we should show preview for |match|.
  bool ShouldShowPreviewFor(const AutocompleteMatch& match);

  // Marks the specified search engine id as not supporting instant.
  void BlacklistFromInstant(TemplateURLID id);

  // Returns true if the specified id has been blacklisted from supporting
  // instant.
  bool IsBlacklistedFromInstant(TemplateURLID id);

  // Clears the set of search engines blacklisted.
  void ClearBlacklist();

  // Returns the TemplateURL to use for the specified AutocompleteMatch, or NULL
  // if non TemplateURL should be used.
  const TemplateURL* GetTemplateURL(const AutocompleteMatch& match);

  InstantDelegate* delegate_;

  // The TabContents last passed to |Update|.
  TabContents* tab_contents_;

  // Has notification been sent out that the preview TabContents is ready to be
  // shown?
  bool is_active_;

  // See description above setter.
  gfx::Rect omnibox_bounds_;

  // See description above CommitOnMouseUp.
  bool commit_on_mouse_up_;

  // See description above getter.
  PageTransition::Type last_transition_type_;

  scoped_ptr<InstantLoaderManager> loader_manager_;

  // The IDs of any search engines that don't support instant. We assume all
  // search engines support instant, but if we determine an engine doesn't
  // support instant it is added to this list. The list is cleared out on every
  // reset/commit.
  std::set<TemplateURLID> blacklisted_ids_;

  base::OneShotTimer<InstantController> update_timer_;

  // URL last pased to ScheduleUpdate.
  GURL scheduled_url_;

  const Type type_;

  DISALLOW_COPY_AND_ASSIGN(InstantController);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
