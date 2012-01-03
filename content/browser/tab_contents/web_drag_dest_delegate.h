// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_WEB_DRAG_DEST_DELEGATE_H_
#define CONTENT_BROWSER_TAB_CONTENTS_WEB_DRAG_DEST_DELEGATE_H_
#pragma once

#if defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#endif  // TOOLKIT_USES_GTK

#include "base/string16.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

class WebContents;

// An optional delegate that listens for drags of bookmark data.
class CONTENT_EXPORT WebDragDestDelegate {
 public:
  // Announces that a drag has started. It's valid that a drag starts, along
  // with over/enter/leave/drop notifications without receiving any bookmark
  // data.
  virtual void DragInitialize(WebContents* contents) = 0;

  // Notifications of drag progression.
  virtual void OnDragOver() = 0;
  virtual void OnDragEnter() = 0;
  virtual void OnDrop() = 0;

#if defined(TOOLKIT_USES_GTK)
  // Returns the bookmark atom type. GTK and Views return different values here.
  virtual GdkAtom GetBookmarkTargetAtom() const = 0;

  // Called when WebDragDestkGtk detects that there's bookmark data in a
  // drag. Not every drag will trigger these.
  virtual void OnReceiveDataFromGtk(GtkSelectionData* data) = 0;
  virtual void OnReceiveProcessedData(const GURL& url,
                                      const string16& title) = 0;
#endif  // TOOLKIT_USES_GTK

  // This should also clear any state kept about this drag.
  virtual void OnDragLeave() = 0;

  virtual ~WebDragDestDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_TAB_CONTENTS_WEB_DRAG_DEST_DELEGATE_H_
