// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTERNAL_POPUP_MENU_H_
#define CHROME_RENDERER_EXTERNAL_POPUP_MENU_H_

#include "base/basictypes.h"
#include "third_party/WebKit/WebKit/chromium/public/WebExternalPopupMenu.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPopupMenuInfo.h"

class RenderView;
namespace WebKit {
class WebExternalPopupMenuClient;
}

class ExternalPopupMenu : public WebKit::WebExternalPopupMenu {
 public:
  ExternalPopupMenu(RenderView* render_view,
                    const WebKit::WebPopupMenuInfo& popup_menu_info,
                    WebKit::WebExternalPopupMenuClient* popup_menu_client);

  // Called when the user has selected an item. |selected_item| is -1 if the
  // user canceled the popup.
  void DidSelectItem(int selected_index);

  // WebKit::WebExternalPopupMenu implementation:
  virtual void show(const WebKit::WebRect& bounds);
  virtual void close();

 private:
  RenderView* render_view_;
  WebKit::WebPopupMenuInfo popup_menu_info_;
  WebKit::WebExternalPopupMenuClient* popup_menu_client_;

  DISALLOW_COPY_AND_ASSIGN(ExternalPopupMenu);
};

#endif  // CHROME_RENDERER_EXTERNAL_POPUP_MENU_H_

