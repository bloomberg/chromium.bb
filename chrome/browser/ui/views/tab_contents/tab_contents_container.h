// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_CONTAINER_H_
#pragma once

// There are two strategies implemented for embedding the actual tab contents
// which are to use a views implementaiton all the way down, or to use a
// NativeViewHost to encapsulate a native widget that then contains another
// views heirarchy rooted at that widget. The TOUCH_UI is currently the only UI
// that uses the pure views approach.
//
// Common code to the two approaches is in tab_contents_container.cc, while
// views-only code is in tab_contents_container_views.cc and native-widget only
// code is in tab_contents_container_native.cc. The headers are distinct
// because the classes have different member variables.

// TODO(oshima): Rename tab_contents_container_native to tab_contents_container.
// TODO(oshima): Figure out if we can consolidate
// native_tab_contents_containers.

#include "chrome/browser/ui/views/tab_contents/tab_contents_container_native.h"

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_CONTAINER_H_
