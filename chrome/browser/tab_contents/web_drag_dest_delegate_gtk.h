// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_DEST_DELEGATE_GTK_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_DEST_DELEGATE_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/string16.h"

class GURL;
class TabContents;

// An optional delegate that listens for drags of bookmark data.
class WebDragDestDelegateGtk {
 public:
  // Announces that a drag has started. It's valid that a drag starts, along
  // with over/enter/leave/drop notifications without receiving any bookmark
  // data.
  virtual void DragInitialize(TabContents* contents) = 0;

  // Returns the bookmark atom type. GTK and Views return different values here.
  virtual GdkAtom GetBookmarkTargetAtom() const = 0;

  // Called when WebDragDestkGtk detects that there's bookmark data in a
  // drag. Not every drag will trigger these.
  virtual void OnReceiveDataFromGtk(GtkSelectionData* data) = 0;
  virtual void OnReceiveProcessedData(const GURL& url,
                                      const string16& title) = 0;

  // Notifications of drag progression.
  virtual void OnDragOver() = 0;
  virtual void OnDragEnter() = 0;
  virtual void OnDrop() = 0;

  // This should also clear any state kept about this drag.
  virtual void OnDragLeave() = 0;

  virtual ~WebDragDestDelegateGtk();
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_DEST_DELEGATE_GTK_H_
