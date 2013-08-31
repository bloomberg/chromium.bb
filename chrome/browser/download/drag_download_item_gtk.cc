// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/drag_download_item.h"

#include "chrome/browser/ui/gtk/download/download_item_drag.h"

void DragDownloadItem(const content::DownloadItem* download,
                      gfx::Image* icon,
                      gfx::NativeView view) {
  DownloadItemDrag::BeginDrag(download, icon);
}
