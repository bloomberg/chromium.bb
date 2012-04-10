// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_DRAG_UTILS_GTK_H_
#define CONTENT_BROWSER_WEB_CONTENTS_DRAG_UTILS_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "content/common/content_export.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"

namespace content {

// Convenience methods for converting between web drag operations and the GDK
// equivalent.
CONTENT_EXPORT GdkDragAction WebDragOpToGdkDragAction(
    WebKit::WebDragOperationsMask op);
CONTENT_EXPORT WebKit::WebDragOperationsMask GdkDragActionToWebDragOp(
    GdkDragAction action);

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_DRAG_UTILS_GTK_H_
