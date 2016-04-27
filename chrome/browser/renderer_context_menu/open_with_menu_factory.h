// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_OPEN_WITH_MENU_FACTORY_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_OPEN_WITH_MENU_FACTORY_H_

#include "base/macros.h"

class RenderViewContextMenuObserver;
class RenderViewContextMenuProxy;

class OpenWithMenuFactory {
 public:
  static RenderViewContextMenuObserver* CreateMenu(
      RenderViewContextMenuProxy* proxy);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(OpenWithMenuFactory);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_OPEN_WITH_MENU_FACTORY_H_
