// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_process_handle.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"

DownloadProcessHandle::DownloadProcessHandle()
    : child_id_(-1), render_view_id_(-1), request_id_(-1) {
}

DownloadProcessHandle::DownloadProcessHandle(int child_id,
                                             int render_view_id,
                                             int request_id)
    : child_id_(child_id),
      render_view_id_(render_view_id),
      request_id_(request_id) {
}

TabContents* DownloadProcessHandle::GetTabContents() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return tab_util::GetTabContentsByID(child_id_, render_view_id_);
}

DownloadManager* DownloadProcessHandle::GetDownloadManager() {
  TabContents* contents = GetTabContents();
  if (!contents)
    return NULL;

  Profile* profile = contents->profile();
  if (!profile)
    return NULL;

  return profile->GetDownloadManager();
}
