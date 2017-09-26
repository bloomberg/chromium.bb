// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/search/search_engine_base_url_tracker.h"
#include "chrome/browser/ui/search/instant_controller.h"
#include "chrome/browser/ui/search/search_delegate.h"
#include "chrome/browser/ui/search/search_model.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}

// BrowserInstantController is responsible for reloading any Instant tabs (which
// today just means NTPs) when the default search provider changes. This can
// happen when the user chooses a different default search engine, or when the
// Google base URL changes while Google is the default search engine. Also owns
// a SearchModel instance that corresponds to the currently-active tab.
class BrowserInstantController {
 public:
  explicit BrowserInstantController(Browser* browser);
  ~BrowserInstantController();

  // Returns the Profile associated with the Browser that owns this object.
  Profile* profile() const;

  InstantController* instant() { return &instant_; }

  SearchModel* search_model() { return &search_model_; }

  // Invoked by |instant_| to get the currently active tab.
  content::WebContents* GetActiveWebContents() const;

  // Invoked by |browser_| when the active tab changes.
  // TODO(treib): Implement TabStripModelObserver instead of relying on custom
  // callbacks from Browser.
  void OnTabActivated(content::WebContents* web_contents);
  void OnTabDeactivated(content::WebContents* web_contents);
  void OnTabDetached(content::WebContents* web_contents);

 private:
  void OnSearchEngineBaseURLChanged(
      SearchEngineBaseURLTracker::ChangeReason change_reason);

  Browser* const browser_;

  // The model for the "active" search state.  There are per-tab search models
  // as well.  When a tab is active its model is kept in sync with this one.
  // When a new tab is activated its model state is propagated to this active
  // model.  This way, observers only have to attach to this single model for
  // updates, and don't have to worry about active tab changes directly.
  SearchModel search_model_;

  // A delegate that handles the details of updating the "active"
  // |search_model_| state with the tab's state.
  SearchDelegate search_delegate_;

  InstantController instant_;

  std::unique_ptr<SearchEngineBaseURLTracker> search_engine_base_url_tracker_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInstantController);
};

#endif  // CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
