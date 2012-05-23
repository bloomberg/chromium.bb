// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEW_TYPE_UTILS_H_
#define CHROME_BROWSER_VIEW_TYPE_UTILS_H_
#pragma once

#include "chrome/common/view_type.h"

namespace content {
class WebContents;
}

namespace chrome {

// Get/Set the type of a WebContents.
// GetViewType handles a NULL |tab| for convenience by returning
// VIEW_TYPE_INVALID.
ViewType GetViewType(content::WebContents* tab);
void SetViewType(content::WebContents* tab, ViewType type);

}  // namespace chrome

#endif  // CHROME_BROWSER_VIEW_TYPE_UTILS_H_
