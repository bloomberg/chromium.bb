// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_SYNC_DB_UTIL_H_
#define COMPONENTS_SYNC_SYNCABLE_SYNC_DB_UTIL_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace base {
class FilePath;
class SingleThreadTaskRunner;
}  // namespace base

namespace syncer {

// Check integrity of sync DB under |sync_dir|. Invoke |callback| with last
// modified time if integrity check passes, with null time otherwise. This
// is called on either sync thread or IO thread.
void CheckSyncDbLastModifiedTime(
    const base::FilePath& sync_dir,
    scoped_refptr<base::SingleThreadTaskRunner> callback_runner,
    base::Callback<void(base::Time)> callback);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_SYNC_DB_UTIL_H_
