// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CUSTOM_MENU_LISTENER_H_
#define CHROME_RENDERER_CUSTOM_MENU_LISTENER_H_
#pragma once

// CustomMenuListener is an interface that can be registered with RenderView by
// classes that wish to use custom context menus. There is a single callback,
// MenuItemSelected(), which fires when a custom menu item was chosen by the
// user.
class CustomMenuListener {
 public:
  virtual void MenuItemSelected(unsigned id) = 0;

 protected:
  virtual ~CustomMenuListener() {}
};

#endif  // CHROME_RENDERER_CUSTOM_MENU_LISTENER_H_

