// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_DELEGATE_H_
#define CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "content/browser/download/download_types.h"
#include "content/public/browser/download_manager_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class DownloadManager;
}

class MockDownloadManagerDelegate : public content::DownloadManagerDelegate {
 public:
  MockDownloadManagerDelegate();
  virtual ~MockDownloadManagerDelegate();

  // DownloadManagerDelegate functions:
  MOCK_METHOD1(SetDownloadManager, void(content::DownloadManager* dm));
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD1(ShouldStartDownload, bool(int32 download_id));
  MOCK_METHOD3(ChooseDownloadPath, void(TabContents* tab_contents,
                                        const FilePath& suggested_path,
                                        void* data));
  MOCK_METHOD2(OverrideIntermediatePath, bool(content::DownloadItem* item,
                                              FilePath* intermediate_path));
  MOCK_METHOD0(GetAlternativeTabContentsToNotifyForDownload,
               content::WebContents*());
  MOCK_METHOD1(ShouldOpenFileBasedOnExtension, bool(const FilePath& path));
  MOCK_METHOD1(ShouldCompleteDownload, bool(content::DownloadItem* item));
  MOCK_METHOD1(ShouldOpenDownload, bool(content::DownloadItem* item));
  MOCK_METHOD0(GenerateFileHash, bool());
  MOCK_METHOD1(OnResponseCompleted, void(content::DownloadItem* item));
  MOCK_METHOD1(AddItemToPersistentStore, void(content::DownloadItem* item));
  MOCK_METHOD1(UpdateItemInPersistentStore, void(content::DownloadItem* item));
  MOCK_METHOD2(UpdatePathForItemInPersistentStore, void(
      content::DownloadItem* item,
      const FilePath& new_path));
  MOCK_METHOD1(RemoveItemFromPersistentStore,
      void(content::DownloadItem* item));
  MOCK_METHOD2(RemoveItemsFromPersistentStoreBetween, void(
      base::Time remove_begin,
      base::Time remove_end));
  MOCK_METHOD3(GetSaveDir, void(TabContents* tab_contents,
                                FilePath* website_save_dir,
                                FilePath* download_save_dir));
  MOCK_METHOD3(ChooseSavePath, void(
      const base::WeakPtr<SavePackage>& save_package,
      const FilePath& suggested_path,
      bool can_save_as_complete));
  MOCK_METHOD0(DownloadProgressUpdated, void());
};

#endif  // CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_DELEGATE_H_
