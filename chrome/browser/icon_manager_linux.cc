// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_manager.h"

#include "base/mime_util.h"
#include "base/threading/thread_restrictions.h"

IconGroupID IconManager::GetGroupIDFromFilepath(const FilePath& filepath) {
  // It turns out the call to mime_util::GetFileMimeType below does IO, but
  // callers of GetGroupIDFromFilepath assume it does not do IO (the Windows
  // and Mac implementations do not). We should fix this by either not doing IO
  // in this method, or reworking callers to avoid calling it on the UI thread.
  // See crbug.com/72740.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  return mime_util::GetFileMimeType(filepath);
}
