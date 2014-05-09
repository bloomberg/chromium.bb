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
#include "chrome/browser/ui/search/search_model_observer.h"

class Browser;
struct InstantSuggestion;
class Profile;

namespace content {
class WebContents;
}

class BrowserInstantController : public SearchModelObserver,
                                 public InstantServiceObserver {
 public:
  explicit BrowserInstantController(Browser* browser);
  virtual ~BrowserInstantController();

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

 private:
  // SearchModelObserver:
  virtual void ModelChanged(const SearchModel::State& old_state,
                            const SearchModel::State& new_state) OVERRIDE;

  // InstantServiceObserver:
  virtual void DefaultSearchProviderChanged() OVERRIDE;

  // Replaces the contents at tab |index| with |new_contents| and deletes the
  // existing contents.
  void ReplaceWebContentsAt(int index,
                            scoped_ptr<content::WebContents> new_contents);

  Browser* const browser_;

  InstantController instant_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInstantController);
};

#endif  // CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
