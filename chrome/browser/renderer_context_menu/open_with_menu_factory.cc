// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/open_with_menu_factory.h"

#if !defined(USE_ASH)
RenderViewContextMenuObserver* OpenWithMenuFactory::CreateMenu(
    RenderViewContextMenuProxy* proxy) {
  return nullptr;
}
#endif  // !defined(USE_ASH)
