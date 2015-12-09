// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/session/blimp_client_session.h"

#include "blimp/client/session/navigation_feature.h"
#include "blimp/client/session/render_widget_feature.h"
#include "blimp/net/browser_connection_handler.h"

namespace blimp {

BlimpClientSession::BlimpClientSession()
    : connection_handler_(new BrowserConnectionHandler),
      navigation_feature_(new NavigationFeature),
      render_widget_feature_(new RenderWidgetFeature) {
  // Connect the features with the network layer.
  navigation_feature_->set_outgoing_message_processor(
      connection_handler_->RegisterFeature(BlimpMessage::NAVIGATION,
                                           navigation_feature_.get()));
  render_widget_feature_->set_outgoing_input_message_processor(
      connection_handler_->RegisterFeature(BlimpMessage::INPUT,
                                           render_widget_feature_.get()));
  render_widget_feature_->set_outgoing_compositor_message_processor(
      connection_handler_->RegisterFeature(BlimpMessage::COMPOSITOR,
                                           render_widget_feature_.get()));
  // We don't expect to send any RenderWidget messages, so don't save the
  // outgoing BlimpMessageProcessor in the RenderWidgetFeature.
  connection_handler_->RegisterFeature(BlimpMessage::RENDER_WIDGET,
                                       render_widget_feature_.get());
}

BlimpClientSession::~BlimpClientSession() {}

NavigationFeature* BlimpClientSession::GetNavigationFeature() const {
  return navigation_feature_.get();
}

RenderWidgetFeature* BlimpClientSession::GetRenderWidgetFeature() const {
  return render_widget_feature_.get();
}

}  // namespace blimp
