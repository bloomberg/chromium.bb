// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/table_row_nsimage_cache.h"

#include "base/logging.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"

TableRowNSImageCache::TableRowNSImageCache(Table* model)
    : model_(model),
      icon_images_([[NSPointerArray alloc] initWithOptions:
          NSPointerFunctionsStrongMemory |
          NSPointerFunctionsObjectPersonality]) {
  int count = model_->RowCount();
  [icon_images_ setCount:count];
}

TableRowNSImageCache::~TableRowNSImageCache() {}

void TableRowNSImageCache::OnModelChanged() {
  int count = model_->RowCount();
  [icon_images_ setCount:0];
  [icon_images_ setCount:count];
}

void TableRowNSImageCache::OnItemsChanged(int start, int length) {
  DCHECK_LE(start + length, static_cast<int>([icon_images_ count]));
  for (int i = start; i < (start + length); ++i) {
    [icon_images_ replacePointerAtIndex:i withPointer:NULL];
  }
  DCHECK_EQ(model_->RowCount(),
            static_cast<int>([icon_images_ count]));
}

void TableRowNSImageCache::OnItemsAdded(int start, int length) {
  DCHECK_LE(start, static_cast<int>([icon_images_ count]));

  // -[NSPointerArray insertPointer:atIndex:] throws if index == count.
  // Instead expand the array with NULLs.
  if (start == static_cast<int>([icon_images_ count])) {
    [icon_images_ setCount:start + length];
  } else {
    for (int i = 0; i < length; ++i) {
      [icon_images_ insertPointer:NULL atIndex:start];  // Values slide up.
    }
  }
  DCHECK_EQ(model_->RowCount(),
            static_cast<int>([icon_images_ count]));
}

void TableRowNSImageCache::OnItemsRemoved(int start, int length) {
  DCHECK_LE(start + length, static_cast<int>([icon_images_ count]));
  for (int i = 0; i < length; ++i) {
    [icon_images_ removePointerAtIndex:start];  // Values slide down.
  }
  DCHECK_EQ(model_->RowCount(),
            static_cast<int>([icon_images_ count]));
}

NSImage* TableRowNSImageCache::GetImageForRow(int row) {
  DCHECK_EQ(model_->RowCount(),
            static_cast<int>([icon_images_ count]));
  DCHECK_GE(row, 0);
  DCHECK_LT(row, static_cast<int>([icon_images_ count]));
  NSImage* image = static_cast<NSImage*>([icon_images_ pointerAtIndex:row]);
  if (!image) {
    const gfx::ImageSkia image_skia_icon =
        model_->GetIcon(row);
    // This means GetIcon() will get called until it returns a non-empty image.
    // Empty images are intentionally not cached.
    if (!image_skia_icon.isNull()) {
      image = gfx::NSImageFromImageSkia(image_skia_icon);
      DCHECK(image);
      [icon_images_ replacePointerAtIndex:row withPointer:image];
    }
  }
  return image;
}

