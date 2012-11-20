// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/prefs/public/pref_observer.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_unload_handler.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "webkit/glue/window_open_disposition.h"

class Browser;
struct InstantSuggestion;
class PrefService;
class Profile;
class TabContents;

namespace gfx {
class Rect;
}

namespace chrome {

class BrowserInstantController : public PrefObserver,
                                 public search::SearchModelObserver {
 public:
  explicit BrowserInstantController(Browser* browser);
  virtual ~BrowserInstantController();

  // Returns true if Instant is enabled in a visible, preview-showing mode.
  static bool IsInstantEnabled(Profile* profile);

  // Registers Instant related preferences.
  static void RegisterUserPrefs(PrefService* prefs);

  // Commits the current Instant, returning true on success. This is intended
  // for use from OpenCurrentURL.
  bool OpenInstant(WindowOpenDisposition disposition);

  // Returns the InstantController or NULL if there is no InstantController for
  // this BrowserInstantController.
  InstantController* instant() { return &instant_; }

  // Invoked by |instant_| to commit the |preview| by merging it into the active
  // tab or adding it as a new tab. We take ownership of |preview|.
  void CommitInstant(TabContents* preview, bool in_new_tab);

  // Invoked by |instant_| to autocomplete the |suggestion| into the omnibox.
  void SetInstantSuggestion(const InstantSuggestion& suggestion);

  // Invoked by |instant_| to get the bounds that the preview is placed at,
  // in screen coordinated.
  gfx::Rect GetInstantBounds();

  // Invoked by |instant_| to notify that the preview gained focus, usually due
  // to the user clicking on it.
  void InstantPreviewFocused();

  // Invoked by |instant_| to get the currently active tab, over which the
  // preview would be shown.
  TabContents* GetActiveTabContents() const;

  // Invoked by |browser_| when the active tab changes.
  void ActiveTabChanged();

 private:
  // Overridden from PrefObserver:
  virtual void OnPreferenceChanged(PrefServiceBase* service,
                                   const std::string& pref_name) OVERRIDE;

  // Overridden from search::SearchModelObserver:
  virtual void ModeChanged(const search::Mode& old_mode,
                           const search::Mode& new_mode) OVERRIDE;

  Browser* const browser_;

  InstantController instant_;
  InstantUnloadHandler instant_unload_handler_;

  PrefChangeRegistrar profile_pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInstantController);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
