// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_FIND_ENTRY_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_FIND_ENTRY_DELEGATE_H_
#pragma once

#include "base/bind.h"
#include "base/platform_file.h"

class FilePath;

namespace gdata {

class GDataEntry;

// Used to get result of file search. Please note that |file| is a live
// object provided to this callback under lock. It must not be used outside
// of the callback method. This callback can be invoked on different thread
// than one that started the request.
typedef base::Callback<void(base::PlatformFileError error,
                            const FilePath& directory_path,
                            GDataEntry* entry)>
    FindEntryCallback;

// Delegate class used to deal with results synchronous read-only search
// over virtual file system.
class FindEntryDelegate {
 public:
  virtual ~FindEntryDelegate();

  // Called when GDataRootDirectory::FindEntryByPath() completes search.
  virtual void OnDone(base::PlatformFileError error,
                      const FilePath& directory_path,
                      GDataEntry* entry) = 0;
};

// Delegate used to find a directory element for file system updates.
class ReadOnlyFindEntryDelegate : public FindEntryDelegate {
 public:
  ReadOnlyFindEntryDelegate();

  // Returns found entry.
  GDataEntry* entry() { return entry_; }

 private:
  // FindEntryDelegate overrides.
  virtual void OnDone(base::PlatformFileError error,
                      const FilePath& directory_path,
                      GDataEntry* entry) OVERRIDE;

  // Entry that was found.
  GDataEntry* entry_;
};

// FindEntryCallbackRelayDelegate class implementation.
// This class is used to relay calls between sync and async versions
// of FindFileByPath(Sync|Async) calls.
class FindEntryCallbackRelayDelegate : public FindEntryDelegate {
 public:
  explicit FindEntryCallbackRelayDelegate(const FindEntryCallback& callback);
  virtual ~FindEntryCallbackRelayDelegate();

 private:
  // FindEntryDelegate overrides.
  virtual void OnDone(base::PlatformFileError error,
                      const FilePath& directory_path,
                      GDataEntry* entry) OVERRIDE;

  const FindEntryCallback callback_;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_FIND_ENTRY_DELEGATE_H_
