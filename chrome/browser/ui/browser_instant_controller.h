// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/ui/search/instant_controller.h"
#include "chrome/browser/ui/search/search_model_observer.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}

class BrowserInstantController : public SearchModelObserver,
                                 public InstantServiceObserver {
 public:
  explicit BrowserInstantController(Browser* browser);
  ~BrowserInstantController() override;

  // Commits the current Instant. This is intended for use from OpenCurrentURL.
  void OpenInstant(WindowOpenDisposition disposition, const GURL& url);

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

 private:
  // SearchModelObserver:
  void ModelChanged(const SearchModel::State& old_state,
                    const SearchModel::State& new_state) override;

  // InstantServiceObserver:
  void DefaultSearchProviderChanged(
      bool google_base_url_domain_changed) override;

  // Replaces the contents at tab |index| with |new_contents| and deletes the
  // existing contents.
  void ReplaceWebContentsAt(int index,
                            std::unique_ptr<content::WebContents> new_contents);

  Browser* const browser_;

  InstantController instant_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInstantController);
};

#endif  // CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
