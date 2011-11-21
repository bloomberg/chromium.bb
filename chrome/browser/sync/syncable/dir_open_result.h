// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_DIR_OPEN_RESULT_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_DIR_OPEN_RESULT_H_
#pragma once

namespace syncable {
enum DirOpenResult { OPENED,   // success.
                     FAILED_NEWER_VERSION,  // DB version is too new.
                     FAILED_MAKE_REPOSITORY,  // Couldn't create subdir.
                     FAILED_OPEN_DATABASE,  // sqlite_open() failed.
                     FAILED_DISK_FULL,  // The disk is full.
                     FAILED_DATABASE_CORRUPT,  // Something is wrong with the DB
                     FAILED_LOGICAL_CORRUPTION, // Invalid database contents
};
}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_DIR_OPEN_RESULT_H_
