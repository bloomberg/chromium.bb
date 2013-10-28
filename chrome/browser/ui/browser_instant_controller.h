// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/ui/search/instant_controller.h"
#include "chrome/browser/ui/search/instant_unload_handler.h"
#include "chrome/browser/ui/search/search_model_observer.h"

class Browser;
struct InstantSuggestion;
class Profile;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

class BrowserInstantController : public SearchModelObserver,
                                 public InstantServiceObserver {
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

  // Invoked by |instant_| to get the currently active tab.
  content::WebContents* GetActiveWebContents() const;

  // Invoked by |browser_| when the active tab changes.
  void ActiveTabChanged();

  // Invoked by |browser_| when the active tab is about to be deactivated.
  void TabDeactivated(content::WebContents* contents);

  // Invoked by |instant_| to paste the |text| (or clipboard content if text is
  // empty) into the omnibox. It will set focus to the omnibox if the omnibox is
  // not focused.
  void PasteIntoOmnibox(const string16& text);

  // Sets the stored omnibox bounds.
  void SetOmniboxBounds(const gfx::Rect& bounds);

  // Notifies |instant_| to toggle voice search.
  void ToggleVoiceSearch();

 private:
  // Overridden from search::SearchModelObserver:
  virtual void ModelChanged(const SearchModel::State& old_state,
                            const SearchModel::State& new_state) OVERRIDE;

  // Overridden from InstantServiceObserver:
  virtual void DefaultSearchProviderChanged() OVERRIDE;
  virtual void GoogleURLUpdated() OVERRIDE;

  // Reloads the tabs in instant process to ensure that their privileged status
  // is still valid.
  void ReloadTabsInInstantProcess();

  // Replaces the contents at tab |index| with |new_contents| and deletes the
  // existing contents.
  void ReplaceWebContentsAt(int index,
                            scoped_ptr<content::WebContents> new_contents);

  Browser* const browser_;

  InstantController instant_;
  InstantUnloadHandler instant_unload_handler_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInstantController);
};

#endif  // CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
