// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_
#define CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_
#pragma once

#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class HistoryService;
class SkBitmap;
struct ThumbnailScore;

namespace history {
class HistoryAddPageArgs;
}

class HistoryTabHelper : public TabContentsObserver,
                         public content::NotificationObserver {
 public:
  explicit HistoryTabHelper(TabContents* tab_contents);
  virtual ~HistoryTabHelper();

  // Updates history with the specified navigation. This is called by
  // OnMsgNavigate to update history state.
  void UpdateHistoryForNavigation(
      scoped_refptr<history::HistoryAddPageArgs> add_page_args);

  // Sends the page title to the history service. This is called when we receive
  // the page title and we know we want to update history.
  void UpdateHistoryPageTitle(const NavigationEntry& entry);

  // Returns the history::HistoryAddPageArgs to use for adding a page to
  // history.
  scoped_refptr<history::HistoryAddPageArgs> CreateHistoryAddPageArgs(
      const GURL& virtual_url,
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params);

 private:
  // TabContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void DidNavigateAnyFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void OnPageContents(const GURL& url,
                      int32 page_id,
                      const string16& contents);
  void OnThumbnail(const GURL& url,
                   const ThumbnailScore& score,
                   const SkBitmap& bitmap);

  // Helper function to return the history service.  May return NULL.
  HistoryService* GetHistoryService();

  // Whether we have a (non-empty) title for the current page.
  // Used to prevent subsequent title updates from affecting history. This
  // prevents some weirdness because some AJAXy apps use titles for status
  // messages.
  bool received_page_title_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(HistoryTabHelper);
};

#endif  // CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_
