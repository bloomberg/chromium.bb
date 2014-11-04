// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"

scoped_ptr<ConstrainedWindowViewsClient>
CreateChromeConstrainedWindowViewsClient() {
  // Mac toolkit-views doesn't use constrained windows yet.
  // TODO(tapted): Delete this file when it does.
  return nullptr;
}
