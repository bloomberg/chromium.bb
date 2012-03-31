// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_ICON_CACHE_H_
#define ASH_APP_LIST_ICON_CACHE_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {
class Size;
}

namespace ash {

// IconCache stores processed image, keyed by the source image and desired size.
class IconCache {
 public:
  static void CreateInstance();
  static void DeleteInstance();

  static IconCache* GetInstance();

  void MarkAllEntryUnused();
  void PurgeAllUnused();

  bool Get(const SkBitmap& src,
           const gfx::Size& size,
           SkBitmap* processed);
  void Put(const SkBitmap& src,
           const gfx::Size& size,
           const SkBitmap& processed);

 private:
  struct Item {
    SkBitmap image;
    bool used;
  };
  typedef std::map<std::string, Item> Cache;

  IconCache();
  ~IconCache();

  static IconCache* instance_;

  Cache cache_;

  DISALLOW_COPY_AND_ASSIGN(IconCache);
};

}  // namespace ash

#endif  // ASH_APP_LIST_ICON_CACHE_H_
