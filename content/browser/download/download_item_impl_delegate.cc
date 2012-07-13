// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/browser/download/download_item_impl_delegate.h"

class DownloadItemImpl;

// Infrastructure in DownloadItemImplDelegate to assert invariant that
// delegate always outlives all attached DownloadItemImpls.
DownloadItemImplDelegate::DownloadItemImplDelegate()
    : count_(0) {}

DownloadItemImplDelegate::~DownloadItemImplDelegate() {
  DCHECK_EQ(0, count_);
}

void DownloadItemImplDelegate::Attach() {
  ++count_;
}

void DownloadItemImplDelegate::Detach() {
  DCHECK_LT(0, count_);
  --count_;
}

bool DownloadItemImplDelegate::ShouldOpenFileBasedOnExtension(
    const FilePath& path) {
  return false;
}

bool DownloadItemImplDelegate::ShouldOpenDownload(DownloadItemImpl* download) {
  return false;
}

void DownloadItemImplDelegate::CheckForFileRemoval(
    DownloadItemImpl* download_item) {}

void DownloadItemImplDelegate::MaybeCompleteDownload(
    DownloadItemImpl* download) {}

content::BrowserContext* DownloadItemImplDelegate::GetBrowserContext() const {
  return NULL;
}

void DownloadItemImplDelegate::DownloadStopped(DownloadItemImpl* download) {}

void DownloadItemImplDelegate::DownloadCompleted(DownloadItemImpl* download) {}

void DownloadItemImplDelegate::DownloadOpened(DownloadItemImpl* download) {}

void DownloadItemImplDelegate::DownloadRemoved(DownloadItemImpl* download) {}

void DownloadItemImplDelegate::DownloadRenamedToIntermediateName(
    DownloadItemImpl* download) {}

void DownloadItemImplDelegate::DownloadRenamedToFinalName(
    DownloadItemImpl* download) {}

void DownloadItemImplDelegate::AssertStateConsistent(
    DownloadItemImpl* download) const {}
