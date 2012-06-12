// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "chrome/browser/instant/instant_commit_type.h"
#include "chrome/browser/instant/instant_loader_delegate.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "chrome/common/instant_types.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

struct AutocompleteMatch;
class InstantDelegate;
class InstantLoader;
class InstantTest;
class PrefService;
class Profile;
class TabContents;
class TemplateURL;

// InstantController maintains a WebContents that is intended to give a preview
// of a URL. InstantController is owned by Browser.
//
// At any time the WebContents maintained by InstantController may be destroyed
// by way of |DestroyPreviewContents|, which results in |HideInstant| being
// invoked on the delegate. Similarly the preview may be committed at any time
// by invoking |CommitCurrentPreview|, which results in |CommitInstant|
// being invoked on the delegate. Also see |PrepareForCommit| below.
class InstantController : public InstantLoaderDelegate {
 public:
  // Amount of time to wait before starting the instant animation.
  static const int kAutoCommitPauseTimeMS = 1000;
  // Duration of the instant animation in which the colors change.
  static const int kAutoCommitFadeInTimeMS = 300;

  // InstantController may operate in one of these modes:
  //   INSTANT: The default search engine is preloaded when the omnibox gets
  //       focus. Queries are issued as the user types. Predicted queries are
  //       are inline autocompleted into the omnibox. Result previews are shown.
  //   SUGGEST: Same as INSTANT, without visible previews.
  //   HIDDEN: Same as SUGGEST, without the inline autocompletion.
  //   SILENT: Same as HIDDEN, without issuing queries as the user types. The
  //       query is sent only after the user presses <Enter>.
  enum Mode {
    INSTANT,
    SUGGEST,
    HIDDEN,
    SILENT
  };

  InstantController(InstantDelegate* delegate, Mode mode);
  virtual ~InstantController();

  // Registers instant related preferences.
  static void RegisterUserPrefs(PrefService* prefs);

  // Records instant metrics.
  static void RecordMetrics(Profile* profile);

  // Returns true if instant is enabled in the given |profile|'s preferences.
  static bool IsEnabled(Profile* profile);

  // Enables instant.
  static void Enable(Profile* profile);

  // Disables instant.
  static void Disable(Profile* profile);

  // Invoked as the user types in the omnibox with the url to navigate to. If
  // the url is valid and a preview WebContents has not been created, it is
  // created. If |verbatim| is true search results are shown for |user_text|
  // rather than the best guess as to what the search thought the user meant.
  // |verbatim| only matters if the AutocompleteMatch is for a search engine
  // that supports instant. Returns true if the attempt to update does not
  // result in the preview WebContents being destroyed.
  bool Update(const AutocompleteMatch& match,
              const string16& user_text,
              bool verbatim,
              string16* suggested_text);

  // Sets the bounds of the omnibox (in screen coordinates). The bounds are
  // remembered until the preview is committed or destroyed. This is only used
  // when showing results for a search provider that supports instant.
  void SetOmniboxBounds(const gfx::Rect& bounds);

  // Notifies the delegate to hide the preview and destroys the preview
  // WebContents. Does nothing if the preview WebContents has not been created.
  void DestroyPreviewContents();

  // Notifies the delegate to hide the preview but leaves it around in hopes it
  // can be subsequently used. The preview will not be used until Update() (with
  // valid parameters) is invoked.
  void Hide();

  // Returns true if we're showing the last URL passed to |Update|. If this is
  // false a commit does not result in committing the last url passed to update.
  // A return value of false happens if we're in the process of determining if
  // the page supports instant.
  bool IsCurrent() const;

  // Returns true if the caller should proceed with committing the preview. A
  // return value of false means that there is no valid preview to commit. This
  // is used by Browser, when the user presses <Enter>, to decide whether to
  // load the omnibox contents through Instant or otherwise. This is needed
  // because calls to |Update| don't necessarily result in a preview being
  // shown, such as in the HIDDEN and SILENT modes.
  bool PrepareForCommit();

  // Invoked when the user does some gesture that should trigger making the
  // current previewed page the permanent page.  Returns the TabContents that
  // contains the committed preview.
  TabContents* CommitCurrentPreview(InstantCommitType type);

  // Accepts the currently showing instant preview, if any, and returns true.
  // Returns false if there is no instant preview showing.
  bool CommitIfCurrent();

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

  // The autocomplete edit has gained focus. Preload the instant URL of the
  // default search engine, in anticipation of the user typing a query.
  void OnAutocompleteGotFocus();

  // Releases the preview WebContents passing ownership to the caller. This is
  // intended to be called when the preview WebContents is committed. This does
  // not notify the delegate. |tab_contents| is the underlying tab onto which
  // the preview will be committed. It can be NULL when the underlying tab is
  // irrelevant, for example when |type| is INSTANT_COMMIT_DESTROY.
  // WARNING: be sure and invoke CompleteRelease after adding the returned
  // WebContents to a tabstrip.
  TabContents* ReleasePreviewContents(InstantCommitType type,
                                      TabContents* tab_contents);

  // Does cleanup after the preview contents has been added to the tabstrip.
  // Invoke this if you explicitly invoke ReleasePreviewContents.
  void CompleteRelease(TabContents* tab);

  // The preview TabContents; may be null.
  TabContents* GetPreviewContents() const;

  // Returns true if the preview TabContents is ready to be displayed. In
  // some situations this may return false yet GetPreviewContents() returns
  // non-NULL.
  bool is_displayable() const { return is_displayable_; }

  // Returns the transition type of the last AutocompleteMatch passed to Update.
  content::PageTransition last_transition_type() const {
    return last_transition_type_;
  }

  // InstantLoaderDelegate
  virtual void InstantStatusChanged(InstantLoader* loader) OVERRIDE;
  virtual void SetSuggestedTextFor(InstantLoader* loader,
                                   const string16& text,
                                   InstantCompleteBehavior behavior) OVERRIDE;
  virtual gfx::Rect GetInstantBounds() OVERRIDE;
  virtual bool ShouldCommitInstantOnMouseUp() OVERRIDE;
  virtual void CommitInstantLoader(InstantLoader* loader) OVERRIDE;
  virtual void InstantLoaderDoesntSupportInstant(
      InstantLoader* loader) OVERRIDE;
  virtual void AddToBlacklist(InstantLoader* loader,
                              const GURL& url) OVERRIDE;
  virtual void SwappedTabContents(InstantLoader* loader) OVERRIDE;
  virtual void InstantLoaderContentsFocused() OVERRIDE;

 private:
  friend class InstantTest;

  typedef std::set<std::string> HostBlacklist;

  // Updates |is_displayable_| and if necessary notifies the delegate.
  void UpdateIsDisplayable();

  // Updates InstantLoaderManager and its current InstantLoader. This is invoked
  // internally from Update.
  void UpdateLoader(TabContents* tab_contents,
                    const TemplateURL* template_url,
                    const GURL& url,
                    content::PageTransition transition_type,
                    const string16& user_text,
                    bool verbatim,
                    string16* suggested_text);

  // Returns true if |template_url| is a valid TemplateURL for use by instant.
  bool IsValidInstantTemplateURL(const TemplateURL* template_url);

  // Marks the loader as not supporting instant.
  void BlacklistFromInstant();

  // Returns true if the specified id has been blacklisted from supporting
  // instant.
  bool IsBlacklistedFromInstant(TemplateURLID id);

  // Clears the set of search engines blacklisted.
  void ClearBlacklist();

  // Deletes |loader| after a delay. At the time we determine a site doesn't
  // want to participate in instant we can't destroy the loader (because
  // destroying the loader destroys the WebContents and the WebContents is on
  // the stack). Instead we place the loader in |loaders_to_destroy_| and
  // schedule a task.
  void ScheduleDestroy(InstantLoader* loader);

  // Destroys all loaders scheduled for destruction in |ScheduleForDestroy|.
  void DestroyLoaders();

  InstantDelegate* delegate_;

  // True if |loader_| is ready to be displayed.
  bool is_displayable_;

  // Set to true in Hide() and false in Update(). Used when we persist the
  // |loader_|, but it isn't up to date.
  bool is_out_of_date_;

  scoped_ptr<InstantLoader> loader_;

  // See description above setter.
  gfx::Rect omnibox_bounds_;

  // See description above CommitOnMouseUp.
  bool commit_on_mouse_up_;

  // See description above getter.
  content::PageTransition last_transition_type_;

  // The IDs of any search engines that don't support instant. We assume all
  // search engines support instant, but if we determine an engine doesn't
  // support instant it is added to this list. The list is cleared out on every
  // reset/commit.
  std::set<TemplateURLID> blacklisted_ids_;

  // Used by ScheduleForDestroy; see it for details.
  base::WeakPtrFactory<InstantController> weak_factory_;

  // List of InstantLoaders to destroy. See ScheduleForDestroy for details.
  ScopedVector<InstantLoader> loaders_to_destroy_;

  // The URL of the most recent match passed to |Update|.
  GURL last_url_;

  // The most recent user_text passed to |Update|.
  string16 last_user_text_;

  // See the enum description above.
  const Mode mode_;

  DISALLOW_COPY_AND_ASSIGN(InstantController);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
