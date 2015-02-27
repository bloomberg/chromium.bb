// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_constants.h"

#include "base/files/file_path.h"

#define FPL FILE_PATH_LITERAL

namespace history {

// filenames
const base::FilePath::CharType kArchivedHistoryFilename[] =
    FPL("Archived History");
const base::FilePath::CharType kFaviconsFilename[] = FPL("Favicons");
const base::FilePath::CharType kHistoryFilename[] = FPL("History");
const base::FilePath::CharType kThumbnailsFilename[] = FPL("Thumbnails");

}  // namespace history
