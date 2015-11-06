// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MD_DOWNLOADS_MD_DOWNLOADS_DOM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MD_DOWNLOADS_MD_DOWNLOADS_DOM_HANDLER_H_

#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/all_download_item_notifier.h"
#include "chrome/browser/download/download_danger_prompt.h"
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
class MdDownloadsDOMHandler : public content::WebUIMessageHandler,
                              public AllDownloadItemNotifier::Observer {
 public:
  explicit MdDownloadsDOMHandler(content::DownloadManager* download_manager);
  ~MdDownloadsDOMHandler() override;

  void Init();

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // AllDownloadItemNotifier::Observer interface
  void OnDownloadCreated(content::DownloadManager* manager,
                         content::DownloadItem* download_item) override;
  void OnDownloadUpdated(content::DownloadManager* manager,
                         content::DownloadItem* download_item) override;
  void OnDownloadRemoved(content::DownloadManager* manager,
                         content::DownloadItem* download_item) override;

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

  // Callback for the "undo" message. Currently only undoes removals.
  void HandleUndo(const base::ListValue* args);

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
  virtual void CallUpdateAll(const base::ListValue& list);
  virtual void CallUpdateItem(const base::DictionaryValue& item);

  // Schedules a call to SendCurrentDownloads() in the next message loop
  // iteration. Protected rather than private for use in tests.
  void ScheduleSendCurrentDownloads();

  // Actually remove downloads with an ID in |removals_|. This cannot be undone.
  void FinalizeRemovals();

 private:
  using IdSet = std::set<uint32>;
  using DownloadVector = std::vector<content::DownloadItem*>;

  // Convenience method to call |main_notifier_->GetManager()| while
  // null-checking |main_notifier_|.
  content::DownloadManager* GetMainNotifierManager() const;

  // Convenience method to call |original_notifier_->GetManager()| while
  // null-checking |original_notifier_|.
  content::DownloadManager* GetOriginalNotifierManager() const;

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
  void DangerPromptDone(int download_id, DownloadDangerPrompt::Action action);

  // Returns true if the records of any downloaded items are allowed (and able)
  // to be deleted.
  bool IsDeletingHistoryAllowed();

  // Returns the download that is referred to in a given value.
  content::DownloadItem* GetDownloadByValue(const base::ListValue* args);

  // Returns the download with |id| or NULL if it doesn't exist.
  content::DownloadItem* GetDownloadById(uint32 id);

  // Remove all downloads in |to_remove| with the ability to undo removal later.
  void RemoveDownloads(const DownloadVector& to_remove);

  // Weak reference to the DownloadManager this class was constructed with. You
  // should probably be using use Get{Main,Original}NotifierManager() instead.
  content::DownloadManager* download_manager_;

  // Current search terms.
  scoped_ptr<base::ListValue> search_terms_;

  // Notifies OnDownload*() and provides safe access to the DownloadManager.
  scoped_ptr<AllDownloadItemNotifier> main_notifier_;

  // If |main_notifier_| observes an incognito profile, then this observes the
  // DownloadManager for the original profile; otherwise, this is NULL.
  scoped_ptr<AllDownloadItemNotifier> original_notifier_;

  // IDs of downloads to remove when this handler gets deleted.
  std::vector<IdSet> removals_;

  // Whether a call to SendCurrentDownloads() is currently scheduled.
  bool update_scheduled_;

  // IDs of new downloads that the page doesn't know about yet.
  IdSet new_downloads_;

  base::WeakPtrFactory<MdDownloadsDOMHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MdDownloadsDOMHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MD_DOWNLOADS_MD_DOWNLOADS_DOM_HANDLER_H_
