// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/render_widget_fullscreen.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"

using WebKit::WebWidget;

// static
RenderWidgetFullscreen* RenderWidgetFullscreen::Create(
    int32 opener_id, RenderThreadBase* render_thread) {
  DCHECK_NE(MSG_ROUTING_NONE, opener_id);
  scoped_refptr<RenderWidgetFullscreen> widget(
      new RenderWidgetFullscreen(render_thread));
  widget->Init(opener_id);
  return widget.release();
}

WebWidget* RenderWidgetFullscreen::CreateWebWidget() {
  // TODO(boliu): Handle full screen render widgets here.
  return RenderWidget::CreateWebWidget(this);
}

void RenderWidgetFullscreen::Init(int32 opener_id) {
  DCHECK(!webwidget_);

  RenderWidget::DoInit(
      opener_id,
      CreateWebWidget(),
      new ViewHostMsg_CreateFullscreenWidget(opener_id, &routing_id_));
}

void RenderWidgetFullscreen::show(WebKit::WebNavigationPolicy) {
  DCHECK(!did_show_) << "received extraneous Show call";
  DCHECK_NE(MSG_ROUTING_NONE, routing_id_);
  DCHECK_NE(MSG_ROUTING_NONE, opener_id_);

  if (!did_show_) {
    did_show_ = true;
    Send(new ViewHostMsg_ShowFullscreenWidget(opener_id_, routing_id_));
    SetPendingWindowRect(initial_pos_);
  }
}

RenderWidgetFullscreen::RenderWidgetFullscreen(RenderThreadBase* render_thread)
    : RenderWidget(render_thread, WebKit::WebPopupTypeNone) {
}
