// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_
#define CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_

#include <memory>

#include "chrome/browser/download/download_ui_model.h"
#include "components/offline_items_collection/core/filtered_offline_item_observer.h"
#include "components/offline_items_collection/core/offline_item.h"

class OfflineItemModelManager;

using offline_items_collection::FilteredOfflineItemObserver;

// Implementation of DownloadUIModel that wrappers around a |OfflineItem|.
class OfflineItemModel : public DownloadUIModel,
                         public FilteredOfflineItemObserver::Observer {
 public:
  // Constructs a OfflineItemModel.
  OfflineItemModel(OfflineItemModelManager* manager,
                   const offline_items_collection::OfflineItem& offline_item);
  ~OfflineItemModel() override;

  // DownloadUIModel implementation.
  int64_t GetCompletedBytes() const override;
  int64_t GetTotalBytes() const override;
  int PercentComplete() const override;
  bool WasUINotified() const override;
  void SetWasUINotified(bool should_notify) override;
  base::FilePath GetTargetFilePath() const override;
  download::DownloadItem::DownloadState GetState() const override;
  bool IsPaused() const override;
  bool TimeRemaining(base::TimeDelta* remaining) const override;
  bool IsDone() const override;
  download::DownloadInterruptReason GetLastReason() const override;
  base::FilePath GetFullPath() const override;
  bool CanResume() const override;
  bool AllDataSaved() const override;
  bool GetFileExternallyRemoved() const override;
  GURL GetURL() const override;

#if !defined(OS_ANDROID)
  bool IsCommandEnabled(const DownloadCommands* download_commands,
                        DownloadCommands::Command command) const override;
  bool IsCommandChecked(const DownloadCommands* download_commands,
                        DownloadCommands::Command command) const override;
  void ExecuteCommand(DownloadCommands* download_commands,
                      DownloadCommands::Command command) override;
#endif

 private:
  // FilteredOfflineItemObserver::Observer overrides.
  void OnItemRemoved(const offline_items_collection::ContentId& id) override;
  void OnItemUpdated(
      const offline_items_collection::OfflineItem& item) override;

  // DownloadUIModel implementation.
  std::string GetMimeType() const override;

  OfflineItemModelManager* manager_;

  std::unique_ptr<FilteredOfflineItemObserver> offline_item_observer_;
  std::unique_ptr<offline_items_collection::OfflineItem> offline_item_;

  DISALLOW_COPY_AND_ASSIGN(OfflineItemModel);
};

#endif  // CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_
