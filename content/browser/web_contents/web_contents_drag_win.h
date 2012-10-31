// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_DRAG_WIN_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_DRAG_WIN_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

class SkBitmap;
struct WebDropData;

namespace gfx {
class ImageSkia;
}

namespace content {
class DragDropThread;
class WebContents;
class WebDragDest;
class WebDragSource;

// Windows-specific drag-and-drop handling in WebContentsView.
// If we are dragging a virtual file out of the browser, we use a background
// thread to do the drag-and-drop because we do not want to run nested
// message loop in the UI thread. For all other cases, the drag-and-drop happens
// in the UI thread.
class CONTENT_EXPORT WebContentsDragWin
    : NON_EXPORTED_BASE(public ui::DataObjectImpl::Observer),
      public base::RefCountedThreadSafe<WebContentsDragWin> {
 public:
  WebContentsDragWin(gfx::NativeWindow source_window,
                     WebContents* web_contents,
                     WebDragDest* drag_dest,
                     const base::Callback<void()>& drag_end_callback);
  virtual ~WebContentsDragWin();

  // Called on UI thread.
  void StartDragging(const WebDropData& drop_data,
                     WebKit::WebDragOperationsMask ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset);
  void CancelDrag();

  // DataObjectImpl::Observer implementation.
  // Called on drag-and-drop thread.
  virtual void OnWaitForData();
  virtual void OnDataObjectDisposed();

 private:
  // Called on either UI thread or drag-and-drop thread.
  void PrepareDragForDownload(const WebDropData& drop_data,
                              ui::OSExchangeData* data,
                              const GURL& page_url,
                              const std::string& page_encoding);
  void PrepareDragForFileContents(const WebDropData& drop_data,
                                  ui::OSExchangeData* data);
  void PrepareDragForUrl(const WebDropData& drop_data,
                         ui::OSExchangeData* data);
  // Returns false if the source window becomes invalid when the drag ends.
  // This could happen when the window gets destroyed when the drag is still in
  // progress. No further processing should be done beyond this return point
  // because the instance has been destroyed.
  bool DoDragging(const WebDropData& drop_data,
                  WebKit::WebDragOperationsMask ops,
                  const GURL& page_url,
                  const std::string& page_encoding,
                  const gfx::ImageSkia& image,
                  const gfx::Vector2d& image_offset);

  // Called on drag-and-drop thread.
  void StartBackgroundDragging(const WebDropData& drop_data,
                               WebKit::WebDragOperationsMask ops,
                               const GURL& page_url,
                               const std::string& page_encoding,
                               const gfx::ImageSkia& image,
                               const gfx::Vector2d& image_offset);
  // Called on UI thread.
  void EndDragging();
  void CloseThread();

  // For debug check only. Access only on drag-and-drop thread.
  base::PlatformThreadId drag_drop_thread_id_;

  // All the member variables below are accessed on UI thread.

  gfx::NativeWindow source_window_;
  WebContents* web_contents_;
  WebDragDest* drag_dest_;

  // |drag_source_| is our callback interface passed to the system when we
  // want to initiate a drag and drop operation.  We use it to tell if a
  // drag operation is happening.
  scoped_refptr<WebDragSource> drag_source_;

  // The thread used by the drag-out download. This is because we want to avoid
  // running nested message loop in main UI thread.
  scoped_ptr<DragDropThread> drag_drop_thread_;

  // The flag to guard that EndDragging is not called twice.
  bool drag_ended_;

  base::Callback<void()> drag_end_callback_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDragWin);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_DRAG_WIN_H_
