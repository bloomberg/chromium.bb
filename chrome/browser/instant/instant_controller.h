// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/browser/instant/instant_commit_type.h"
#include "chrome/browser/instant/instant_loader_delegate.h"
#include "chrome/common/instant_types.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

struct AutocompleteMatch;
class AutocompleteProvider;
class InstantControllerDelegate;
class InstantLoader;
class PrefService;
class Profile;
class TabContents;
class TemplateURL;

// InstantController maintains a WebContents that is intended to give a
// preview of search results. InstantController is owned by Browser via
// BrowserInstantController.
//
// At any time the WebContents maintained by InstantController may be hidden
// from view by way of Hide(), which may result in HideInstant() being invoked
// on the delegate. Similarly the preview may be committed at any time by
// invoking CommitCurrentPreview(), which results in CommitInstant() being
// invoked on the delegate.
class InstantController : public InstantLoaderDelegate {
 public:
  // Amount of time to wait before starting the animation for suggested text.
  static const int kInlineAutocompletePauseTimeMS = 1000;

  // Duration of the suggested text animation in which the colors change.
  static const int kInlineAutocompleteFadeInTimeMS = 300;

  // InstantController may operate in one of these modes:
  //   EXTENDED: The default search engine is preloaded when the omnibox gets
  //       focus. Queries are issued as the user types. Predicted queries are
  //       inline autocompleted into the omnibox. Previews of search results
  //       as well as predicted URLs are shown. Search suggestions are rendered
  //       within the search results preview.
  //   INSTANT: Same as EXTENDED, without URL previews. Search suggestions are
  //       rendered by the omnibox drop down, and not by the preview page.
  //   SUGGEST: Same as INSTANT, without visible previews.
  //   HIDDEN: Same as SUGGEST, without the inline autocompletion.
  //   SILENT: Same as HIDDEN, without issuing queries as the user types. The
  //       query is sent only after the user presses <Enter>.
  //   DISABLED: Instant is disabled.
  enum Mode {
    EXTENDED,
    INSTANT,
    SUGGEST,
    HIDDEN,
    SILENT,
    DISABLED,
  };

  virtual ~InstantController();

  // Creates a new InstantController. Caller owns the returned object. The
  // |profile| pointer is not cached, so the underlying profile object need not
  // live beyond this call. ***NOTE***: May return NULL, which means that
  // Instant is disabled in this profile.
  static InstantController* CreateInstant(Profile* profile,
                                          InstantControllerDelegate* delegate);

  // Returns true if Instant is enabled and supports the extended API.
  static bool IsExtendedAPIEnabled(Profile* profile);

  // Returns true if Instant is enabled in a visible, preview-showing mode.
  static bool IsInstantEnabled(Profile* profile);

  // Returns true if Instant will provide autocomplete suggestions.
  static bool IsSuggestEnabled(Profile* profile);

  // Registers Instant related preferences.
  static void RegisterUserPrefs(PrefService* prefs);

  // Invoked as the user types into the omnibox. |user_text| is what the user
  // has typed. |full_text| is what the omnibox is showing. These may differ if
  // the user typed only some text, and the rest was inline autocompleted. If
  // |verbatim| is true, search results are shown for the exact omnibox text,
  // rather than the best guess as to what the user means. Returns true if the
  // update is accepted (i.e., if |match| is a search rather than a URL).
  bool Update(const AutocompleteMatch& match,
              const string16& user_text,
              const string16& full_text,
              bool verbatim);

  // Sets the bounds of the omnibox dropdown, in screen coordinates.
  void SetOmniboxBounds(const gfx::Rect& bounds);

  // Send autocomplete results from |providers| to the preview page.
  void HandleAutocompleteResults(
      const std::vector<AutocompleteProvider*>& providers);

  // The preview TabContents. May be NULL if ReleasePreviewContents() has been
  // called, with no subsequent successful call to Update(). InstantController
  // retains ownership of the object.
  TabContents* GetPreviewContents() const;

  // Hides the preview, but doesn't destroy it, in hopes it can be subsequently
  // reused. The preview will not be used until a call to Update() succeeds.
  void Hide();

  // Returns true if the Instant preview can be committed now. This can be true
  // even if the preview is not showing yet, because we can commit as long as
  // we've processed the last Update() and we know the loader supports Instant.
  bool IsCurrent() const;

  // Unconditionally commits the preview. Returns the TabContents that contains
  // the committed preview.
  TabContents* CommitCurrentPreview(InstantCommitType type);

  // Releases the preview WebContents passing ownership to the caller. This is
  // intended to be called when the preview WebContents is committed. This does
  // not notify the delegate.
  TabContents* ReleasePreviewContents(
      InstantCommitType type) WARN_UNUSED_RESULT;

  // The autocomplete edit that was initiating the current Instant session has
  // lost focus. Commit or discard the preview accordingly.
  void OnAutocompleteLostFocus(gfx::NativeView view_gaining_focus);

  // The autocomplete edit has gained focus. Preload the Instant URL of the
  // default search engine, in anticipation of the user typing a query.
  void OnAutocompleteGotFocus();

  // Returns whether the preview will be committed when the mouse or touch
  // pointer is released.
  bool commit_on_pointer_release() const;

  // Returns the transition type of the last AutocompleteMatch passed to Update.
  content::PageTransition last_transition_type() const {
    return last_transition_type_;
  }

  // InstantLoaderDelegate:
  virtual void SetSuggestions(
      InstantLoader* loader,
      const std::vector<InstantSuggestion>& suggestions) OVERRIDE;
  virtual void CommitInstantLoader(InstantLoader* loader) OVERRIDE;
  virtual void InstantLoaderPreviewLoaded(InstantLoader* loader) OVERRIDE;
  virtual void InstantSupportDetermined(InstantLoader* loader,
                                        bool supports_instant) OVERRIDE;
  virtual void SwappedTabContents(InstantLoader* loader) OVERRIDE;
  virtual void InstantLoaderContentsFocused(InstantLoader* loader) OVERRIDE;

#if defined(UNIT_TEST)
  // Accessors used only in tests.
  bool is_showing() const { return is_showing_; }
  InstantLoader* loader() const { return loader_.get(); }
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(InstantTest, InstantLoaderRefresh);

  InstantController(InstantControllerDelegate* delegate, Mode mode);

  // Creates a new loader if necessary (for example, if the |instant_url| has
  // changed since the last time we created the loader).
  void ResetLoader(const std::string& instant_url,
                   const TabContents* active_tab);

  // Ensures that the |loader_| uses the default Instant URL, recreating it if
  // necessary, and returns true. Returns false if the Instant URL could not be
  // determined or the active tab is NULL (browser is shutting down).
  bool CreateDefaultLoader();

  // If the |loader_| is not showing, it is deleted and recreated. Else the
  // refresh is skipped and the next refresh is scheduled.
  void OnStaleLoader();

  // Calls OnStaleLoader if |stale_loader_timer_| is not running.
  void MaybeOnStaleLoader();

  // Destroys the |loader_| and its preview contents.
  void DeleteLoader();

  // Counterpart to Hide(). Asks the |delegate_| to display the preview.
  void Show();

  // Send the omnibox dropdown bounds to the page.
  void SendBoundsToPage();

  // If |template_url| is a valid TemplateURL for use with Instant, fills in
  // |instant_url| and returns true; returns false otherwise.
  // Note: If the command-line switch kInstantURL is set, this method uses its
  // value for |instant_url| and returns true without examining |template_url|.
  bool GetInstantURL(const TemplateURL* template_url,
                     std::string* instant_url) const;

  // Returns true if the preview is no longer relevant, say because the last
  // Update() was for a URL and not a search query, or the user switched tabs.
  bool IsOutOfDate() const;

  InstantControllerDelegate* const delegate_;

  scoped_ptr<InstantLoader> loader_;

  // See the enum description above.
  const Mode mode_;

  // The active tab at the time of the last Update(). Used by IsOutOfDate() to
  // know whether the user switched tabs. ***NEVER DEREFERENCE THIS POINTER.***
  // It may be a dangling pointer to a freed object. Should only be used for
  // pointer comparisons.
  const void* last_active_tab_;

  // The most recent full_text passed to Update().
  string16 last_full_text_;

  // The most recent user_text passed to Update().
  string16 last_user_text_;

  // The most recent verbatim passed to Update().
  bool last_verbatim_;

  // The most recent suggestion received from the page, minus any prefix that
  // the user has typed.
  InstantSuggestion last_suggestion_;

  // See comments on the getter above.
  content::PageTransition last_transition_type_;

  // True if the last match passed to Update() was a search (versus a URL).
  bool last_match_was_search_;

  // True if the preview is currently being displayed. Guaranteed to be false
  // if IsOutOfDate() is true.
  bool is_showing_;

  // True if we've received a response from the loader for the last Update(),
  // thus indicating that the page is ready to be shown.
  bool loader_processed_last_update_;

  // Current omnibox bounds.
  gfx::Rect omnibox_bounds_;

  // Last bounds passed to the page.
  gfx::Rect last_omnibox_bounds_;

  // Timer used to update the bounds of the omnibox.
  base::OneShotTimer<InstantController> update_bounds_timer_;

  // Timer used to ensure that the Instant page does not get too stale.
  base::OneShotTimer<InstantController> stale_loader_timer_;

  // For each key K => value N, the map says that we found that the search
  // engine identified by Instant URL K didn't support the Instant API in each
  // of the last N times that we loaded it. If an Instant URL isn't present in
  // the map at all or has a value 0, it means that search engine supports the
  // Instant API (or we assume it does, since we haven't determined it doesn't).
  std::map<std::string, int> blacklisted_urls_;

  // Search terms extraction (for autocomplete history matches) doesn't work
  // on Instant URLs. So, whenever the user commits an Instant search, we add
  // an equivalent non-Instant search URL to history, so that the search shows
  // up in autocomplete history matches.
  GURL url_for_history_;

  DISALLOW_COPY_AND_ASSIGN(InstantController);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
