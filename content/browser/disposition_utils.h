// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DISPOSITION_UTILS_H_
#define CONTENT_BROWSER_DISPOSITION_UTILS_H_
#pragma once

#include "content/common/content_export.h"
#include "webkit/glue/window_open_disposition.h"

namespace disposition_utils {

// Translates event flags from a click on a link into the user's desired
// window disposition.  For example, a middle click would mean to open
// a background tab.
CONTENT_EXPORT WindowOpenDisposition DispositionFromClick(bool middle_button,
                                                          bool alt_key,
                                                          bool ctrl_key,
                                                          bool meta_key,
                                                          bool shift_key);

}

#endif  // CONTENT_BROWSER_DISPOSITION_UTILS_H_
