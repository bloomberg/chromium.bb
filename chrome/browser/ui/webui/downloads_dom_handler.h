// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOM_HANDLER_H_

#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/all_download_item_notifier.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace content {
class WebContents;
}

// The handler for Javascript messages related to the "downloads" view,
// also observes changes to the download manager.
class DownloadsDOMHandler : public content::WebUIMessageHandler,
                            public AllDownloadItemNotifier::Observer {
 public:
  explicit DownloadsDOMHandler(content::DownloadManager* dlm);
  virtual ~DownloadsDOMHandler();

  void Init();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // AllDownloadItemNotifier::Observer interface
  virtual void OnDownloadCreated(
      content::DownloadManager* manager,
      content::DownloadItem* download_item) OVERRIDE;
  virtual void OnDownloadUpdated(
      content::DownloadManager* manager,
      content::DownloadItem* download_item) OVERRIDE;
  virtual void OnDownloadRemoved(
      content::DownloadManager* manager,
      content::DownloadItem* download_item) OVERRIDE;

  // Callback for the "onPageLoaded" message.
  void OnPageLoaded(const base::ListValue* args);

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

  // Callback for the "resume" message - resumes the file download.
  void HandleResume(const base::ListValue* args);

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

 protected:
  // These methods are for mocking so that most of this class does not actually
  // depend on WebUI. The other methods that depend on WebUI are
  // RegisterMessages() and HandleDrag().
  virtual content::WebContents* GetWebUIWebContents();
  virtual void CallDownloadsList(const base::ListValue& downloads);
  virtual void CallDownloadUpdated(const base::ListValue& download);

 private:
  // Shorthand for |observing_items_|, which tracks all items that this is
  // observing so that RemoveObserver will be called for all of them.
  typedef std::set<content::DownloadItem*> DownloadSet;

  // Schedules a call to SendCurrentDownloads() in the next message loop
  // iteration.
  void ScheduleSendCurrentDownloads();

  // Sends the current list of downloads to the page.
  void SendCurrentDownloads();

  // Displays a native prompt asking the user for confirmation after accepting
  // the dangerous download specified by |dangerous|. The function returns
  // immediately, and will invoke DangerPromptAccepted() asynchronously if the
  // user accepts the dangerous download. The native prompt will observe
  // |dangerous| until either the dialog is dismissed or |dangerous| is no
  // longer an in-progress dangerous download.
  void ShowDangerPrompt(content::DownloadItem* dangerous);

  // Conveys danger acceptance from the DownloadDangerPrompt to the
  // DownloadItem.
  void DangerPromptAccepted(int download_id);

  // Returns the download that is referred to in a given value.
  content::DownloadItem* GetDownloadByValue(const base::ListValue* args);

  // Current search text.
  string16 search_text_;

  // Notifies OnDownload*() and provides safe access to the DownloadManager.
  AllDownloadItemNotifier main_notifier_;

  // If |main_notifier_| observes an incognito profile, then this observes the
  // DownloadManager for the original profile; otherwise, this is NULL.
  scoped_ptr<AllDownloadItemNotifier> original_notifier_;

  // Whether a call to SendCurrentDownloads() is currently scheduled.
  bool update_scheduled_;

  base::WeakPtrFactory<DownloadsDOMHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsDOMHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOM_HANDLER_H_
