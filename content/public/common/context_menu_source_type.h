// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTEXT_MENU_SOURCE_TYPE_H_
#define CONTENT_PUBLIC_COMMON_CONTEXT_MENU_SOURCE_TYPE_H_

namespace content {

enum ContextMenuSourceType {
  CONTEXT_MENU_SOURCE_MOUSE,
  CONTEXT_MENU_SOURCE_KEYBOARD,
  CONTEXT_MENU_SOURCE_TOUCH,

  // This source type is used when the context menu is summoned by pressing the
  // context menu button in the touch editing menu.
  CONTEXT_MENU_SOURCE_TOUCH_EDIT_MENU,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONTEXT_MENU_SOURCE_TYPE_H_
