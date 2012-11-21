// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_H_

#include "chrome/browser/sessions/session_id.h"

namespace browser_sync {
class SyncedWindowDelegate;
}

namespace content {
class WebContents;
}

class Profile;

// Abstract representation of a Tab Model for Android.  Since Android does
// not use Browser/BrowserList, this is required to allow Chrome to interact
// with Android's Tabs and Tab Model.
class TabModel {
 public:
  TabModel();
  virtual ~TabModel();

  virtual SessionID::id_type GetSessionId() const = 0;
  virtual int GetTabCount() const = 0;
  virtual int GetActiveIndex() const = 0;
  virtual content::WebContents* GetWebContentsAt(int index) const = 0;
  virtual SessionID::id_type GetTabIdAt(int index) const = 0;

  // Used for restoring tabs from synced foreign sessions.
  virtual void CreateTab(content::WebContents* web_contents) = 0;

  // Return true if we are currently restoring sessions asynchronously.
  virtual bool IsSessionRestoreInProgress() const = 0;

  virtual void OpenClearBrowsingData() const = 0;
  virtual Profile* GetProfile() const = 0;
  virtual bool IsOffTheRecord() const = 0;

  virtual browser_sync::SyncedWindowDelegate* GetSyncedWindowDelegate() = 0;

 protected:
  // Instructs the TabModel to broadcast a notification that all tabs are now
  // loaded from storage.
  void BroadcastSessionRestoreComplete();

 private:
  DISALLOW_COPY_AND_ASSIGN(TabModel);
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_H_
