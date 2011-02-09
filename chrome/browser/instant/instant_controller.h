// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "base/string16.h"
#include "base/task.h"
#include "base/timer.h"
#include "chrome/browser/instant/instant_commit_type.h"
#include "chrome/browser/instant/instant_loader_delegate.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "chrome/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

struct AutocompleteMatch;
class InstantDelegate;
class InstantLoader;
class InstantLoaderManager;
class InstantTest;
class PrefService;
class Profile;
class TabContents;
class TabContentsWrapper;
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
  // Amount of time to wait before starting the instant animation.
  static const int kAutoCommitPauseTimeMS = 1000;
  // Duration of the instant animation in which the colors change.
  static const int kAutoCommitFadeInTimeMS = 300;

  InstantController(Profile* profile, InstantDelegate* delegate);
  ~InstantController();

  // Registers instant related preferences.
  static void RegisterUserPrefs(PrefService* prefs);

  // Records instant metrics.
  static void RecordMetrics(Profile* profile);

  // Returns true if instant is enabled.
  static bool IsEnabled(Profile* profile);

  // Enables instant.
  static void Enable(Profile* profile);

  // Disables instant.
  static void Disable(Profile* profile);

  // Accepts the currently showing instant preview, if any, and returns true.
  // Returns false if there is no instant preview showing.
  static bool CommitIfCurrent(InstantController* controller);

  // Invoked as the user types in the omnibox with the url to navigate to.  If
  // the url is empty and there is a preview TabContents it is destroyed. If url
  // is non-empty and the preview TabContents has not been created it is
  // created. If |verbatim| is true search results are shown for |user_text|
  // rather than the best guess as to what the search thought the user meant.
  // |verbatim| only matters if the AutocompleteMatch is for a search engine
  // that supports instant.
  void Update(TabContentsWrapper* tab_contents,
              const AutocompleteMatch& match,
              const string16& user_text,
              bool verbatim,
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
  TabContentsWrapper* ReleasePreviewContents(InstantCommitType type);

  // Does cleanup after the preview contents has been added to the tabstrip.
  // Invoke this if you explicitly invoke ReleasePreviewContents.
  void CompleteRelease(TabContents* tab);

  // TabContents the match is being shown for.
  TabContentsWrapper* tab_contents() const { return tab_contents_; }

  // The preview TabContents; may be null.
  TabContentsWrapper* GetPreviewContents();

  // Returns true if |Update| has been invoked without a corresponding call to
  // |DestroyPreviewContents| or |CommitCurrentPreview|.
  bool is_active() const { return is_active_; }

  // Returns true if the preview TabContents is ready to be displayed. In some
  // situations this may return false yet GetPreviewContents() returns non-NULL.
  bool is_displayable() const { return is_displayable_; }

  // Returns the transition type of the last AutocompleteMatch passed to Update.
  PageTransition::Type last_transition_type() const {
    return last_transition_type_;
  }

  // Returns true if we're showing results from a provider that supports the
  // instant API. See description of |MightSupportInstant| for how this
  // differs from actual loading state.
  bool IsShowingInstant();

  // Returns true if we're attempting to use the instant API with the last URL
  // passed to |Update|. The value of this may change if it turns the provider
  // doesn't really support the instant API.
  // The value of |IsShowingInstant| indicates whether what is currently
  // displayed supports instant, whereas this returns the loading state.  The
  // state of |IsShowingInstant| differs when transitioning from a non-search
  // provider to a search provider that supports instant (or the other way
  // around). For example, if |Update| is passed www.foo.com, followed by a
  // search string then this returns true, but |IsShowingInstant| returns false
  // (until the search provider loads, then both return true).
  bool MightSupportInstant();

  // Returns the URL currently being loaded or shown if everything has finished
  // loading.
  GURL GetCurrentURL();

  // InstantLoaderDelegate
  virtual void ShowInstantLoader(InstantLoader* loader);
  virtual void SetSuggestedTextFor(InstantLoader* loader,
                                   const string16& text);
  virtual gfx::Rect GetInstantBounds();
  virtual bool ShouldCommitInstantOnMouseUp();
  virtual void CommitInstantLoader(InstantLoader* loader);
  virtual void InstantLoaderDoesntSupportInstant(InstantLoader* loader);
  virtual void AddToBlacklist(InstantLoader* loader, const GURL& url);

 private:
  friend class InstantTest;

  typedef std::set<std::string> HostBlacklist;

  // Destroys the current loaders but remains actives.
  void DestroyAndLeaveActive();

  // Returns the TabContents of the pending loader (or NULL). This is only used
  // for testing.
  TabContentsWrapper* GetPendingPreviewContents();

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
                    bool verbatim,
                    string16* suggested_text);

  // Returns true if a preview should be shown for |match|. If |match| has
  // a TemplateURL that supports the instant API it is set in |template_url|.
  bool ShouldShowPreviewFor(const AutocompleteMatch& match,
                            const TemplateURL** template_url);

  // Marks the specified search engine id as not supporting instant.
  void BlacklistFromInstant(TemplateURLID id);

  // Returns true if the specified id has been blacklisted from supporting
  // instant.
  bool IsBlacklistedFromInstant(TemplateURLID id);

  // Clears the set of search engines blacklisted.
  void ClearBlacklist();

  // Deletes |loader| after a delay. At the time we determine a site doesn't
  // want to participate in instant we can't destroy the loader (because
  // destroying the loader destroys the TabContents and the TabContents is on
  // the stack). Instead we place the loader in |loaders_to_destroy_| and
  // schedule a task.
  void ScheduleDestroy(InstantLoader* loader);

  // Destroys all loaders scheduled for destruction in |ScheduleForDestroy|.
  void DestroyLoaders();

  // Returns the TemplateURL to use for the specified AutocompleteMatch, or
  // NULL if there is no TemplateURL for |match|.
  const TemplateURL* GetTemplateURL(const AutocompleteMatch& match);

  InstantDelegate* delegate_;

  // The TabContents last passed to |Update|.
  TabContentsWrapper* tab_contents_;

  // See description above getter for details.
  bool is_active_;

  // Has notification been sent out that the preview TabContents is ready to be
  // shown?
  bool is_displayable_;

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

  // Used by ScheduleForDestroy; see it for details.
  ScopedRunnableMethodFactory<InstantController> destroy_factory_;

  // URL last pased to ScheduleUpdate.
  GURL scheduled_url_;

  // List of InstantLoaders to destroy. See ScheduleForDestroy for details.
  ScopedVector<InstantLoader> loaders_to_destroy_;

  // The set of hosts that we don't use instant with. This is shared across all
  // instances and only maintained for the current session.
  static HostBlacklist* host_blacklist_;

  DISALLOW_COPY_AND_ASSIGN(InstantController);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
