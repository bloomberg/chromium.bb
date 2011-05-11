// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PROCESS_HANDLE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PROCESS_HANDLE_H_
#pragma once

class DownloadManager;
class ResourceDispatcherHost;
class TabContents;

// A handle used by the download system for operations on external
// objects associated with the download (e.g. URLRequest, TabContents,
// DownloadManager).
// This class needs to be copyable, so we can pass it across threads and not
// worry about lifetime or const-ness.
class DownloadProcessHandle {
 public:
  DownloadProcessHandle();
  DownloadProcessHandle(int child_id, int render_view_id, int request_id);

  // These functions must be called on the UI thread.
  TabContents* GetTabContents();
  DownloadManager* GetDownloadManager();

  int child_id() const { return child_id_; }
  int render_view_id() const { return render_view_id_; }
  int request_id() const { return request_id_; }

 private:
  // The ID of the child process that started the download.
  int child_id_;

  // The ID of the render view that started the download.
  int render_view_id_;

  // The ID associated with the request used for the download.
  int request_id_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PROCESS_HANDLE_H_
