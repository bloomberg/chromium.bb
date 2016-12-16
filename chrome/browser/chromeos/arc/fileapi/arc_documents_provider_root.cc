// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace arc {

ArcDocumentsProviderRoot::ArcDocumentsProviderRoot(
    const std::string& authority,
    const std::string& root_document_id) {}

ArcDocumentsProviderRoot::~ArcDocumentsProviderRoot() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void ArcDocumentsProviderRoot::GetFileInfo(
    const base::FilePath& path,
    const GetFileInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTIMPLEMENTED();  // TODO(crbug.com/671511): Implement this function.
}

void ArcDocumentsProviderRoot::ReadDirectory(
    const base::FilePath& path,
    const ReadDirectoryCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTIMPLEMENTED();  // TODO(crbug.com/671511): Implement this function.
}

}  // namespace arc
