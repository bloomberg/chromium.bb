// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_unload_handler.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/window_open_disposition.h"

class Browser;
struct InstantSuggestion;
class PrefRegistrySyncable;
class PrefService;
class Profile;
class ThemeService;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

namespace chrome {

class BrowserInstantController : public content::NotificationObserver,
                                 public search::SearchModelObserver {
 public:
  explicit BrowserInstantController(Browser* browser);
  virtual ~BrowserInstantController();

  // Returns true if Instant is enabled in a visible, preview-showing mode.
  static bool IsInstantEnabled(Profile* profile);

  // Registers Instant related preferences.
  static void RegisterUserPrefs(PrefService* prefs,
                                PrefRegistrySyncable* registry);

  // If |url| is the new tab page URL, set |target_contents| to the preloaded
  // NTP contents from InstantController. If |source_contents| is not NULL, we
  // replace it with the new |target_contents| in the tabstrip and delete
  // |source_contents|. Otherwise, the caller owns |target_contents| and is
  // responsible for inserting it into the tabstrip.
  //
  // Returns true if and only if we update |target_contents|.
  bool MaybeSwapInInstantNTPContents(
      const GURL& url,
      content::WebContents* source_contents,
      content::WebContents** target_contents);

  // Commits the current Instant, returning true on success. This is intended
  // for use from OpenCurrentURL.
  bool OpenInstant(WindowOpenDisposition disposition);

  // Returns the Profile associated with the Browser that owns this object.
  Profile* profile() const;

  // Returns the InstantController or NULL if there is no InstantController for
  // this BrowserInstantController.
  InstantController* instant() { return &instant_; }

  // Invoked by |instant_| to commit the |preview| by merging it into the active
  // tab or adding it as a new tab.
  void CommitInstant(scoped_ptr<content::WebContents> preview, bool in_new_tab);

  // Invoked by |instant_| to autocomplete the |suggestion| into the omnibox.
  void SetInstantSuggestion(const InstantSuggestion& suggestion);

  // Invoked by |instant_| to commit the omnibox's suggested text.
  // Call-through to OmniboxEditModel::CommitSuggestedText.
  void CommitSuggestedText(bool skip_inline_autocomplete);

  // Invoked by |instant_| to get the bounds that the preview is placed at,
  // in screen coordinates.
  gfx::Rect GetInstantBounds();

  // Invoked by |instant_| to notify that the preview gained focus, usually due
  // to the user clicking on it.
  void InstantPreviewFocused();

  // Invoked by |instant_| to give the omnibox focus invisibly.
  void FocusOmniboxInvisibly();

  // Invoked by |instant_| to get the currently active tab, over which the
  // preview would be shown.
  content::WebContents* GetActiveWebContents() const;

  // Invoked by |browser_| when the active tab changes.
  void ActiveTabChanged();

  // Invoked by |browser_| when the active tab is about to be deactivated.
  void TabDeactivated(content::WebContents* contents);

  // Invoked by |instant_| or |browser_| to update theme information for NTP.
  // Set |parse_theme_info| to true to force re-parsing of theme information.
  void UpdateThemeInfo(bool parse_theme_info);

  // Invoked by the InstantController when it wants to open a URL.
  void OpenURL(const GURL& url,
               content::PageTransition transition,
               WindowOpenDisposition disposition);

  // Sets the stored omnibox bounds.
  void SetOmniboxBounds(const gfx::Rect& bounds);

 private:
  // Sets the value of |instant_| based on value from profile. Invoked
  // on pref change.
  void ResetInstant();

  // Overridden from search::SearchModelObserver:
  virtual void ModeChanged(const search::Mode& old_mode,
                           const search::Mode& new_mode) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Helper for handling theme change.
  void OnThemeChanged(ThemeService* theme_service);

  // Replaces the contents at tab |index| with |new_contents| and deletes the
  // existing contents.
  void ReplaceWebContentsAt(int index,
                            scoped_ptr<content::WebContents> new_contents);

  Browser* const browser_;

  InstantController instant_;
  InstantUnloadHandler instant_unload_handler_;

  // Theme-related data for NTP preview to adopt themes.
  bool initialized_theme_info_;  // True if theme_info_ has been initialized.
  ThemeBackgroundInfo theme_info_;

  PrefChangeRegistrar profile_pref_registrar_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInstantController);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
