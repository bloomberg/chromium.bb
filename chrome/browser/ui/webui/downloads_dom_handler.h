// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOM_HANDLER_H_
#pragma once

#include <vector>

#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/webui/web_ui.h"

namespace base {
class ListValue;
}

// The handler for Javascript messages related to the "downloads" view,
// also observes changes to the download manager.
class DownloadsDOMHandler : public WebUIMessageHandler,
                            public DownloadManager::Observer,
                            public DownloadItem::Observer {
 public:
  explicit DownloadsDOMHandler(DownloadManager* dlm);
  virtual ~DownloadsDOMHandler();

  void Init();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // DownloadItem::Observer interface
  virtual void OnDownloadUpdated(DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(DownloadItem* download) OVERRIDE { }

  // DownloadManager::Observer interface
  virtual void ModelChanged() OVERRIDE;

  // Callback for the "getDownloads" message.
  void HandleGetDownloads(const base::ListValue* args);

  // Callback for the "openFile" message - opens the file in the shell.
  void HandleOpenFile(const base::ListValue* args);

  // Callback for the "drag" message - initiates a file object drag.
  void HandleDrag(const base::ListValue* args);

  // Callback for the "saveDangerous" message - specifies that the user
  // wishes to save a dangerous file.
  void HandleSaveDangerous(const base::ListValue* args);

  // Callback for the "discardDangerous" message - specifies that the user
  // wishes to discard (remove) a dangerous file.
  void HandleDiscardDangerous(const base::ListValue* args);

  // Callback for the "show" message - shows the file in explorer.
  void HandleShow(const base::ListValue* args);

  // Callback for the "pause" message - pauses the file download.
  void HandlePause(const base::ListValue* args);

  // Callback for the "remove" message - removes the file download from shelf
  // and list.
  void HandleRemove(const base::ListValue* args);

  // Callback for the "cancel" message - cancels the download.
  void HandleCancel(const base::ListValue* args);

  // Callback for the "clearAll" message - clears all the downloads.
  void HandleClearAll(const base::ListValue* args);

  // Callback for the "openDownloadsFolder" message - opens the downloads
  // folder.
  void HandleOpenDownloadsFolder(const base::ListValue* args);

 private:
  class OriginalDownloadManagerObserver;

  // Send the current list of downloads to the page.
  void SendCurrentDownloads();

  // Clear all download items and their observers.
  void ClearDownloadItems();

  // Return the download that corresponds to a given id.
  DownloadItem* GetDownloadById(int id);

  // Return the download that is referred to in a given value.
  DownloadItem* GetDownloadByValue(const base::ListValue* args);

  // Current search text.
  std::wstring search_text_;

  // Our model
  DownloadManager* download_manager_;

  // The downloads webui for an off-the-record window also shows downloads from
  // the parent profile.
  scoped_ptr<OriginalDownloadManagerObserver>
      original_download_manager_observer_;

  // The current set of visible DownloadItems for this view received from the
  // DownloadManager. DownloadManager owns the DownloadItems. The vector is
  // kept in order, sorted by ascending start time.
  // Note that when a download item is removed, the entry in the vector becomes
  // null.  This should only be a transient state, as a ModelChanged()
  // notification should follow close on the heels of such a change.
  typedef std::vector<DownloadItem*> OrderedDownloads;
  OrderedDownloads download_items_;

  base::ScopedCallbackFactory<DownloadsDOMHandler> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsDOMHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOM_HANDLER_H_
