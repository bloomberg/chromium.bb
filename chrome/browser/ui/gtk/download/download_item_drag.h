// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_ITEM_DRAG_H_
#define CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_ITEM_DRAG_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/custom_drag.h"

namespace content {
class DownloadItem;
}

// Encapsulates functionality for drags of download items.
class DownloadItemDrag : public CustomDrag {
 public:
  // Sets |widget| as a source for drags pertaining to |item|. No
  // DownloadItemDrag object is created.
  // It is safe to call this multiple times with different values of |icon|.
  static void SetSource(GtkWidget* widget,
                        const content::DownloadItem* item,
                        gfx::Image* icon);

  // Creates a new DownloadItemDrag, the lifetime of which is tied to the
  // system drag.
  static void BeginDrag(const content::DownloadItem* item, gfx::Image* icon);

 private:
  class DragData;

  DownloadItemDrag(const content::DownloadItem* item, gfx::Image* icon);
  virtual ~DownloadItemDrag();

  virtual void OnDragDataGet(GtkWidget* widget,
                             GdkDragContext* context,
                             GtkSelectionData* selection_data,
                             guint target_type,
                             guint time) OVERRIDE;

  scoped_ptr<DragData> drag_data_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemDrag);
};


#endif  // CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_ITEM_DRAG_H_
