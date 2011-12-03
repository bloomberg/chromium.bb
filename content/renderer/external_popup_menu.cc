// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/external_popup_menu.h"

#include "content/common/view_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebExternalPopupMenuClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"

ExternalPopupMenu::ExternalPopupMenu(
    RenderViewImpl* render_view,
    const WebKit::WebPopupMenuInfo& popup_menu_info,
    WebKit::WebExternalPopupMenuClient* popup_menu_client)
    : render_view_(render_view),
      popup_menu_info_(popup_menu_info),
      popup_menu_client_(popup_menu_client) {
}

void ExternalPopupMenu::show(const WebKit::WebRect& bounds) {
  ViewHostMsg_ShowPopup_Params popup_params;
  popup_params.bounds = bounds;
  popup_params.item_height = popup_menu_info_.itemHeight;
  popup_params.item_font_size = popup_menu_info_.itemFontSize;
  popup_params.selected_item = popup_menu_info_.selectedIndex;
  for (size_t i = 0; i < popup_menu_info_.items.size(); ++i)
    popup_params.popup_items.push_back(WebMenuItem(popup_menu_info_.items[i]));
  popup_params.right_aligned = popup_menu_info_.rightAligned;
  render_view_->Send(
      new ViewHostMsg_ShowPopup(render_view_->routing_id(), popup_params));
}

void ExternalPopupMenu::close()  {
  popup_menu_client_ = NULL;
  render_view_ = NULL;
}

void ExternalPopupMenu::DidSelectItem(int index) {
  if (!popup_menu_client_)
    return;
  if (index == -1)
    popup_menu_client_->didCancel();
  else
    popup_menu_client_->didAcceptIndex(index);
}
