// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/ui/search/instant_controller.h"
#include "chrome/browser/ui/search/instant_unload_handler.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "ui/base/window_open_disposition.h"

class Browser;
struct InstantSuggestion;
class Profile;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

class BrowserInstantController : public SearchModelObserver {
 public:
  explicit BrowserInstantController(Browser* browser);
  virtual ~BrowserInstantController();

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
  bool OpenInstant(WindowOpenDisposition disposition, const GURL& url);

  // Returns the Profile associated with the Browser that owns this object.
  Profile* profile() const;

  // Returns the InstantController or NULL if there is no InstantController for
  // this BrowserInstantController.
  InstantController* instant() { return &instant_; }

  // Invoked by |instant_| to change the omnibox focus.
  void FocusOmnibox(OmniboxFocusState state);

  // Invoked by |instant_| to get the currently active tab.
  content::WebContents* GetActiveWebContents() const;

  // Invoked by |browser_| when the active tab changes.
  void ActiveTabChanged();

  // Invoked by |browser_| when the active tab is about to be deactivated.
  void TabDeactivated(content::WebContents* contents);

  // Invoked by the InstantController when it wants to open a URL.
  void OpenURL(const GURL& url,
               content::PageTransition transition,
               WindowOpenDisposition disposition);

  // Invoked by |instant_| to paste the |text| (or clipboard content if text is
  // empty) into the omnibox. It will set focus to the omnibox if the omnibox is
  // not focused.
  void PasteIntoOmnibox(const string16& text);

  // Sets the stored omnibox bounds.
  void SetOmniboxBounds(const gfx::Rect& bounds);

  // Sets the current query to prefetch if any.
  void SetSuggestionToPrefetch(const InstantSuggestion& suggestion);

  // Notifies |instant_| to toggle voice search.
  void ToggleVoiceSearch();

 private:
  // Overridden from search::SearchModelObserver:
  virtual void ModelChanged(const SearchModel::State& old_state,
                            const SearchModel::State& new_state) OVERRIDE;

  // Called when the default search provider changes. Revokes the searchbox API
  // privileges for any existing WebContents (that belong to the erstwhile
  // default search provider) by simply reloading all such WebContents. This
  // ensures that they are reloaded in a non-privileged renderer process.
  void OnDefaultSearchProviderChanged(const std::string& pref_name);

  // Replaces the contents at tab |index| with |new_contents| and deletes the
  // existing contents.
  void ReplaceWebContentsAt(int index,
                            scoped_ptr<content::WebContents> new_contents);

  Browser* const browser_;

  InstantController instant_;
  InstantUnloadHandler instant_unload_handler_;

  PrefChangeRegistrar profile_pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInstantController);
};

#endif  // CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
