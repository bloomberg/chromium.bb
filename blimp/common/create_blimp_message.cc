// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/create_blimp_message.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/input.pb.h"
#include "blimp/common/proto/render_widget.pb.h"

namespace blimp {

scoped_ptr<BlimpMessage> CreateBlimpMessage(
    CompositorMessage** compositor_message,
    int target_tab_id) {
  DCHECK(compositor_message);
  scoped_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::COMPOSITOR);
  output->set_target_tab_id(target_tab_id);
  *compositor_message = output->mutable_compositor();
  return output;
}

scoped_ptr<BlimpMessage> CreateBlimpMessage(InputMessage** input_message) {
  DCHECK(input_message);
  scoped_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::INPUT);
  *input_message = output->mutable_input();
  return output;
}

scoped_ptr<BlimpMessage> CreateBlimpMessage(
    RenderWidgetMessage** render_widget_message,
    int target_tab_id) {
  DCHECK(render_widget_message);
  scoped_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::RENDER_WIDGET);
  output->set_target_tab_id(target_tab_id);
  *render_widget_message = output->mutable_render_widget();
  return output;
}

scoped_ptr<BlimpMessage> CreateStartConnectionMessage(
    const std::string& client_token,
    int protocol_version) {
  scoped_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::PROTOCOL_CONTROL);

  ProtocolControlMessage* control_message = output->mutable_protocol_control();
  control_message->set_type(ProtocolControlMessage::START_CONNECTION);

  StartConnectionMessage* start_connection_message =
      control_message->mutable_start_connection();
  start_connection_message->set_client_token(client_token);
  start_connection_message->set_protocol_version(protocol_version);

  return output;
}

}  // namespace blimp
