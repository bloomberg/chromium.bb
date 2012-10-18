// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/api/prefs/pref_change_registrar.h"
#include "chrome/browser/instant/instant_controller_delegate.h"
#include "chrome/browser/instant/instant_unload_handler.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/instant_types.h"
#include "content/public/browser/notification_observer.h"
#include "webkit/glue/window_open_disposition.h"

class Browser;
class InstantController;
class InstantTest;
class TabContents;

namespace content {
class NotificationDetails;
class NotificationSource;
}

namespace gfx {
class Rect;
}

namespace chrome {

class BrowserInstantController : public InstantControllerDelegate,
                                 public TabStripModelObserver,
                                 public search::SearchModelObserver,
                                 public content::NotificationObserver {
 public:
  explicit BrowserInstantController(Browser* browser);
  virtual ~BrowserInstantController();

  // Commits the current Instant, returning true on success. This is intended
  // for use from OpenCurrentURL.
  bool OpenInstant(WindowOpenDisposition disposition);

  // Returns the InstantController or NULL if there is no InstantController for
  // this BrowserInstantController.
  InstantController* instant() const { return instant_.get(); }

 private:
  // Overridden from InstantControllerDelegate:
  virtual void CommitInstant(TabContents* preview, bool in_new_tab) OVERRIDE;
  virtual void SetSuggestedText(const string16& text,
                                InstantCompleteBehavior behavior) OVERRIDE;
  virtual gfx::Rect GetInstantBounds() OVERRIDE;
  virtual void InstantPreviewFocused() OVERRIDE;
  virtual TabContents* GetActiveTabContents() const OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from TabStripModelObserver:
  virtual void TabDeactivated(TabContents* contents) OVERRIDE;

  // Overridden from search::SearchModelObserver:
  virtual void ModeChanged(const search::Mode& old_mode,
                           const search::Mode& new_mode) OVERRIDE;

  // If this browser should have Instant, a new InstantController created;
  // otherwise any existing InstantController is destroyed.
  void ResetInstant();

  Browser* browser_;

  scoped_ptr<InstantController> instant_;
  InstantUnloadHandler instant_unload_handler_;

  PrefChangeRegistrar profile_pref_registrar_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserInstantController);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
