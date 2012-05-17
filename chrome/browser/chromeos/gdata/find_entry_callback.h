// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_FIND_ENTRY_CALLBACK_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_FIND_ENTRY_CALLBACK_H_
#pragma once

#include "base/callback.h"
#include "base/platform_file.h"

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

// Callback used to find a directory element for file system updates.
void ReadOnlyFindEntryCallback(GDataEntry** out,
                               base::PlatformFileError error,
                               const FilePath& directory_path,
                               GDataEntry* entry);

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_FIND_ENTRY_CALLBACK_H_
