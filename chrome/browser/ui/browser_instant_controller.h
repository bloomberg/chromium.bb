// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/instant/instant_controller_delegate.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "webkit/glue/window_open_disposition.h"

class Browser;
class InstantController;
class InstantUnloadHandler;
class TabContents;

namespace chrome {

class BrowserInstantController : public InstantControllerDelegate,
                                 public TabStripModelObserver,
                                 public content::NotificationObserver {
 public:
  explicit BrowserInstantController(Browser* browser);
  virtual ~BrowserInstantController();

  // Commits the current instant, returning true on success. This is intended
  // for use from OpenCurrentURL.
  bool OpenInstant(WindowOpenDisposition disposition);

  // Returns the InstantController or NULL if there is no InstantController for
  // this BrowserInstantController.
  InstantController* instant() const { return instant_.get(); }

 private:
  // Overridden from InstantControllerDelegate:
  virtual void ShowInstant(TabContents* preview_contents) OVERRIDE;
  virtual void HideInstant() OVERRIDE;
  virtual void CommitInstant(TabContents* preview_contents) OVERRIDE;
  virtual void SetSuggestedText(const string16& text,
                                InstantCompleteBehavior behavior) OVERRIDE;
  virtual gfx::Rect GetInstantBounds() OVERRIDE;
  virtual void InstantPreviewFocused() OVERRIDE;
  virtual TabContents* GetInstantHostTabContents() const OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from TabStripModelObserver:
  virtual void TabDeactivated(TabContents* contents) OVERRIDE;

  // If this browser should have instant one is created, otherwise does nothing.
  void CreateInstantIfNecessary();

  Browser* browser_;

  scoped_ptr<InstantController> instant_;
  scoped_ptr<InstantUnloadHandler> instant_unload_handler_;

  PrefChangeRegistrar profile_pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInstantController);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
