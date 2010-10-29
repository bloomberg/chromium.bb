// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_MAC_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_MAC_H_
#pragma once

#include "chrome/browser/extensions/extension_host.h"

class RenderWidgetHostView;

class ExtensionHostMac : public ExtensionHost {
 public:
  ExtensionHostMac(const Extension* extension, SiteInstance* site_instance,
                   const GURL& url, ViewType::Type host_type) :
      ExtensionHost(extension, site_instance, url, host_type) {}
  virtual ~ExtensionHostMac();
 protected:
  virtual RenderWidgetHostView* CreateNewWidgetInternal(
      int route_id,
      WebKit::WebPopupType popup_type);
  virtual void ShowCreatedWidgetInternal(RenderWidgetHostView* widget_host_view,
                                         const gfx::Rect& initial_pos);
 private:
  virtual void UnhandledKeyboardEvent(const NativeWebKeyboardEvent& event);

  DISALLOW_COPY_AND_ASSIGN(ExtensionHostMac);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_MAC_H_
