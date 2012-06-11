// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EVENT_DISPOSITION_H_
#define CHROME_BROWSER_EVENT_DISPOSITION_H_
#pragma once

#include "webkit/glue/window_open_disposition.h"

namespace browser {

// Translates event flags into what kind of disposition they represents.
// For example, a middle click would mean to open a background tab.
// event_flags are the flags as understood by views::MouseEvent.
WindowOpenDisposition DispositionFromEventFlags(int event_flags);

}  // namespace browser

#endif  // CHROME_BROWSER_EVENT_DISPOSITION_H_
