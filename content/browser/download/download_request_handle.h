// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_HANDLE_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_HANDLE_H_
#pragma once

#include <string>

class DownloadManager;
class ResourceDispatcherHost;
class TabContents;

// A handle used by the download system for operations on the URLRequest
// or objects conditional on it (e.g. TabContents).
// This class needs to be copyable, so we can pass it across threads and not
// worry about lifetime or const-ness.
class DownloadRequestHandle {
 public:
  // Create a null DownloadRequestHandle: getters will return null, and
  // all actions are no-ops.
  // TODO(rdsmith): Ideally, actions would be forbidden rather than
  // no-ops, to confirm that no non-testing code actually uses
  // a null DownloadRequestHandle.  But for now, we need the no-op
  // behavior for unit tests.  Long-term, this should be fixed by
  // allowing mocking of ResourceDispatcherHost in unit tests.
  DownloadRequestHandle();

  // Note that |rdh| is required to be non-null.
  DownloadRequestHandle(ResourceDispatcherHost* rdh,
                        int child_id,
                        int render_view_id,
                        int request_id);

  // These functions must be called on the UI thread.
  TabContents* GetTabContents() const;
  DownloadManager* GetDownloadManager() const;

  // Pause or resume the matching URL request.  Note that while these
  // do not modify the DownloadRequestHandle, they do modify the
  // request the handle refers to.
  void PauseRequest() const;
  void ResumeRequest() const;

  // Cancel the request.  Note that while this does not
  // modify the DownloadRequestHandle, it does modify the
  // request the handle refers to.
  void CancelRequest() const;

  std::string DebugString() const;

 private:
  // The resource dispatcher host.
  ResourceDispatcherHost* rdh_;

  // The ID of the child process that started the download.
  int child_id_;

  // The ID of the render view that started the download.
  int render_view_id_;

  // The ID associated with the request used for the download.
  int request_id_;
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_HANDLE_H_
