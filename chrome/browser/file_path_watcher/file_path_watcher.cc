// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Cross platform methods for FilePathWatcher. See the various platform
// specific implementation files, too.

#include "chrome/browser/file_path_watcher/file_path_watcher.h"

FilePathWatcher::~FilePathWatcher() {
  impl_->Cancel();
}

bool FilePathWatcher::Watch(const FilePath& path,
                            Delegate* delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(path.IsAbsolute());
  return impl_->Watch(path, delegate);
}
