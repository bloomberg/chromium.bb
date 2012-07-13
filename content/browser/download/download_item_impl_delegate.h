// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_IMPL_DELEGATE_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_IMPL_DELEGATE_H_

#include "base/file_path.h"
#include "content/common/content_export.h"

class DownloadItemImpl;

namespace content {
class BrowserContext;
}

// Delegate for operations that a DownloadItemImpl can't do for itself.
// The base implementation of this class does nothing (returning false
// on predicates) so interfaces not of interest to a derived class may
// be left unimplemented.
class CONTENT_EXPORT DownloadItemImplDelegate {
 public:
  DownloadItemImplDelegate();
  virtual ~DownloadItemImplDelegate();

  // Used for catching use-after-free errors.
  void Attach();
  void Detach();

  // Tests if a file type should be opened automatically.
  virtual bool ShouldOpenFileBasedOnExtension(const FilePath& path);

  // Allows the delegate to override the opening of a download. If it returns
  // true then it's reponsible for opening the item.
  virtual bool ShouldOpenDownload(DownloadItemImpl* download);

  // Checks whether a downloaded file still exists and updates the
  // file's state if the file is already removed.
  // The check may or may not result in a later asynchronous call
  // to OnDownloadedFileRemoved().
  virtual void CheckForFileRemoval(DownloadItemImpl* download_item);

  // If all pre-requisites have been met, complete download processing.
  // TODO(rdsmith): Move into DownloadItem.
  virtual void MaybeCompleteDownload(DownloadItemImpl* download);

  // For contextual issues like language and prefs.
  virtual content::BrowserContext* GetBrowserContext() const;

  // Handle any delegate portions of a state change operation on the
  // DownloadItem.
  virtual void DownloadStopped(DownloadItemImpl* download);
  virtual void DownloadCompleted(DownloadItemImpl* download);
  virtual void DownloadOpened(DownloadItemImpl* download);
  virtual void DownloadRemoved(DownloadItemImpl* download);
  virtual void DownloadRenamedToIntermediateName(
      DownloadItemImpl* download);
  virtual void DownloadRenamedToFinalName(DownloadItemImpl* download);

  // Assert consistent state for delgate object at various transitions.
  virtual void AssertStateConsistent(DownloadItemImpl* download) const;

 private:
  // For "Outlives attached DownloadItemImpl" invariant assertion.
  int count_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemImplDelegate);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_IMPL_DELEGATE_H_
