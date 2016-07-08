// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_OBSERVER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_OBSERVER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT IndexedDBObserver {
 public:
  IndexedDBObserver(int32_t observer_id);
  ~IndexedDBObserver();

  int32_t id() const { return observer_id_; }

 private:
  int32_t observer_id_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_OBSERVER_H_
