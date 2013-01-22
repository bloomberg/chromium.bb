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
class PrefServiceSyncable;
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
  static void RegisterUserPrefs(PrefServiceSyncable* prefs);

  // Commits the current Instant, returning true on success. This is intended
  // for use from OpenCurrentURL.
  bool OpenInstant(WindowOpenDisposition disposition);

  // Returns the InstantController or NULL if there is no InstantController for
  // this BrowserInstantController.
  InstantController* instant() { return &instant_; }

  // Invoked by |instant_| to commit the |preview| by merging it into the active
  // tab or adding it as a new tab. We take ownership of |preview|.
  void CommitInstant(content::WebContents* preview, bool in_new_tab);

  // Invoked by |instant_| to autocomplete the |suggestion| into the omnibox.
  void SetInstantSuggestion(const InstantSuggestion& suggestion);

  // Invoked by |instant_| to get the bounds that the preview is placed at,
  // in screen coordinated.
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

  // Invoked by |BrowserWindow| during layout to set content height which is
  // used as theme area height, i.e. the height of the area that the entire
  // theme background image should fill up.
  void SetContentHeight(int height);

  // Invoked by |instant_| to update theme information for preview.
  void UpdateThemeInfoForPreview();

  // Invoked by the InstantController when it wants to open a URL.
  void OpenURLInCurrentTab(const GURL& url, content::PageTransition transition);

  // Sets the start and end margins of the omnibox text area.
  void SetMarginSize(int start, int end);

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

  // Helper for handling theme area height change.
  void OnThemeAreaHeightChanged(int height);

  Browser* const browser_;

  InstantController instant_;
  InstantUnloadHandler instant_unload_handler_;

  // Theme-related data for NTP preview to adopt themes.
  bool initialized_theme_info_;  // True if theme_info_ has been initialized.
  ThemeBackgroundInfo theme_info_;
  int theme_area_height_;

  PrefChangeRegistrar profile_pref_registrar_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInstantController);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
