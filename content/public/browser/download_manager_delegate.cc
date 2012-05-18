// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_manager_delegate.h"

#include "content/public/browser/download_id.h"

namespace content {

DownloadManagerDelegate::~DownloadManagerDelegate() {
}

DownloadId DownloadManagerDelegate::GetNextId() {
  return DownloadId::Invalid();
}

bool DownloadManagerDelegate::ShouldStartDownload(int32 download_id) {
  return true;
}

FilePath DownloadManagerDelegate::GetIntermediatePath(
    const FilePath& suggested_path) {
  return suggested_path;
}

WebContents* DownloadManagerDelegate::
    GetAlternativeWebContentsToNotifyForDownload() {
  return NULL;
}

bool DownloadManagerDelegate::ShouldOpenFileBasedOnExtension(
    const FilePath& path) {
  return false;
}

bool DownloadManagerDelegate::ShouldCompleteDownload(
    DownloadItem* item,
    const base::Closure& complete_callback) {
  return true;
}

bool DownloadManagerDelegate::ShouldOpenDownload(DownloadItem* item) {
  return true;
}

bool DownloadManagerDelegate::GenerateFileHash() {
  return false;
}

}  // namespace content
