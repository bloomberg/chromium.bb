// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/storage/privet_filesystem_constants.h"

namespace local_discovery {

const base::FilePath::CharType kPrivetFilePath[] = FILE_PATH_LITERAL("/privet");
const char kPrivetTypeStorage[] = "storage";

const char kPrivetListEntries[] = "entries";
const char kPrivetListKeyName[] = "name";
const char kPrivetListKeySize[] = "size";
const char kPrivetListKeyType[] = "type";
const char kPrivetListTypeDir[] = "dir";

}  // namespace local_discovery
