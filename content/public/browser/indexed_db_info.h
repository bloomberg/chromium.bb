// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_INDEXED_DB_INFO_H_
#define CONTENT_PUBLIC_BROWSER_INDEXED_DB_INFO_H_

#include "base/time.h"
#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"

namespace content {

class CONTENT_EXPORT IndexedDBInfo {
 public:
  IndexedDBInfo(const GURL& origin,
                int64 size,
                const base::Time& last_modified);

  GURL origin;
  int64 size;
  base::Time last_modified;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_INDEXED_DB_INFO_H_
