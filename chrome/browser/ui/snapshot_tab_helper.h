// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SNAPSHOT_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SNAPSHOT_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class SkBitmap;

// Per-tab class to handle snapshot functionality.
class SnapshotTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SnapshotTabHelper> {
 public:
  virtual ~SnapshotTabHelper();

  // Captures a snapshot of the page.
  void CaptureSnapshot();

 private:
  explicit SnapshotTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SnapshotTabHelper>;

  // content::WebContentsObserver overrides:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Internal helpers ----------------------------------------------------------

  // Message handler.
  void OnSnapshot(const SkBitmap& bitmap);

  DISALLOW_COPY_AND_ASSIGN(SnapshotTabHelper);
};

#endif  // CHROME_BROWSER_UI_SNAPSHOT_TAB_HELPER_H_
