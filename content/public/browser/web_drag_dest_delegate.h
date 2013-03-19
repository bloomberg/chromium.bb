// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_DRAG_DEST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_DRAG_DEST_DELEGATE_H_

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif  // TOOLKIT_GTK

#include "base/string16.h"

#if defined(OS_WIN)
#include "ui/base/dragdrop/drop_target_win.h"
#endif

class GURL;
struct WebDropData;

namespace ui {
class OSExchangeData;
}

namespace content {

class WebContents;

// An optional delegate that listens for drags of bookmark data.
class WebDragDestDelegate {
 public:
  // Announces that a drag has started. It's valid that a drag starts, along
  // with over/enter/leave/drop notifications without receiving any bookmark
  // data.
  virtual void DragInitialize(WebContents* contents) = 0;

  // Notifications of drag progression.
#if defined(OS_WIN) && !defined(USE_AURA)
  virtual void OnDragOver(IDataObject* data_object) = 0;
  virtual void OnDragEnter(IDataObject* data_object) = 0;
  virtual void OnDrop(IDataObject* data_object) = 0;
  virtual void OnDragLeave(IDataObject* data_object) = 0;
#else
  virtual void OnDragOver() = 0;
  virtual void OnDragEnter() = 0;
  virtual void OnDrop() = 0;
  // This should also clear any state kept about this drag.
  virtual void OnDragLeave() = 0;
#endif

#if defined(TOOLKIT_GTK)
  // Returns the bookmark atom type. GTK and Views return different values here.
  virtual GdkAtom GetBookmarkTargetAtom() const = 0;

  // Called when WebDragDestkGtk detects that there's bookmark data in a
  // drag. Not every drag will trigger these.
  virtual void OnReceiveDataFromGtk(GtkSelectionData* data) = 0;
  virtual void OnReceiveProcessedData(const GURL& url,
                                      const string16& title) = 0;
#elif defined(USE_AURA)
  // Called at the start of every drag to supply the data associated with the
  // drag.
  virtual void OnReceiveDragData(const ui::OSExchangeData& data) = 0;
#elif defined(OS_WIN)
  // Allows the delegate to set data on the drag. If it doesn't want to set
  // data, it should return false.
  virtual bool AddDragData(const WebDropData& drop_data,
                           ui::OSExchangeData* data) = 0;
#endif  // TOOLKIT_GTK

  virtual ~WebDragDestDelegate() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_DRAG_DEST_DELEGATE_H_
