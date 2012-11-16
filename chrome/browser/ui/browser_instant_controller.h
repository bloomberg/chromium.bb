// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/prefs/public/pref_observer.h"
#include "base/string16.h"
#include "chrome/browser/instant/instant_unload_handler.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/instant_types.h"
#include "webkit/glue/window_open_disposition.h"

class Browser;
class InstantController;
class InstantTest;
class TabContents;

namespace gfx {
class Rect;
}

namespace chrome {

class BrowserInstantController : public TabStripModelObserver,
                                 public search::SearchModelObserver,
                                 public PrefObserver {
 public:
  explicit BrowserInstantController(Browser* browser);
  virtual ~BrowserInstantController();

  // Commits the current Instant, returning true on success. This is intended
  // for use from OpenCurrentURL.
  bool OpenInstant(WindowOpenDisposition disposition);

  // Returns the InstantController or NULL if there is no InstantController for
  // this BrowserInstantController.
  InstantController* instant() const { return instant_.get(); }

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

  // Overridden from PrefObserver:
  virtual void OnPreferenceChanged(PrefServiceBase* service,
                                   const std::string& pref_name) OVERRIDE;

  // Overridden from TabStripModelObserver:
  virtual void ActiveTabChanged(content::WebContents* old_contents,
                                content::WebContents* new_contents,
                                int index,
                                bool user_gesture) OVERRIDE;
  virtual void TabStripEmpty() OVERRIDE;

  // Overridden from search::SearchModelObserver:
  virtual void ModeChanged(const search::Mode& old_mode,
                           const search::Mode& new_mode) OVERRIDE;

 private:
  // If this browser should have Instant, a new InstantController created;
  // otherwise any existing InstantController is destroyed.
  void ResetInstant();

  Browser* browser_;

  scoped_ptr<InstantController> instant_;
  InstantUnloadHandler instant_unload_handler_;

  PrefChangeRegistrar profile_pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInstantController);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
