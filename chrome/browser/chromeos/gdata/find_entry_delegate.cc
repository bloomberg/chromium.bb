// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/find_entry_delegate.h"

#include "chrome/browser/chromeos/gdata/gdata_files.h"

namespace gdata {

FindEntryDelegate::~FindEntryDelegate() {
}

ReadOnlyFindEntryDelegate::ReadOnlyFindEntryDelegate() : entry_(NULL) {
}

void ReadOnlyFindEntryDelegate::OnDone(base::PlatformFileError error,
                                       const FilePath& directory_path,
                                       GDataEntry* entry) {
  DCHECK(!entry_);
  if (error == base::PLATFORM_FILE_OK)
    entry_ = entry;
  else
    entry_ = NULL;
}

FindEntryCallbackRelayDelegate::FindEntryCallbackRelayDelegate(
    const FindEntryCallback& callback) : callback_(callback) {
}

FindEntryCallbackRelayDelegate::~FindEntryCallbackRelayDelegate() {
}

void FindEntryCallbackRelayDelegate::OnDone(base::PlatformFileError error,
                                            const FilePath& directory_path,
                                            GDataEntry* entry) {
  if (!callback_.is_null())
    callback_.Run(error, directory_path, entry);
}

}  // namespace gdata
