// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/create_blimp_message.h"

#include <memory>

#include "base/logging.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/input.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "blimp/common/proto/settings.pb.h"
#include "blimp/common/proto/tab_control.pb.h"

namespace blimp {

std::unique_ptr<BlimpMessage> CreateBlimpMessage(
    CompositorMessage** compositor_message,
    int target_tab_id) {
  DCHECK(compositor_message);
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::COMPOSITOR);
  output->set_target_tab_id(target_tab_id);
  *compositor_message = output->mutable_compositor();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(
    TabControlMessage** control_message) {
  DCHECK(control_message);
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::TAB_CONTROL);
  *control_message = output->mutable_tab_control();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(InputMessage** input_message) {
  DCHECK(input_message);
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::INPUT);
  *input_message = output->mutable_input();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(
    NavigationMessage** navigation_message,
    int target_tab_id) {
  DCHECK(navigation_message);
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::NAVIGATION);
  output->set_target_tab_id(target_tab_id);
  *navigation_message = output->mutable_navigation();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(ImeMessage** ime_message,
                                                 int target_tab_id) {
  DCHECK(ime_message);
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::IME);
  output->set_target_tab_id(target_tab_id);
  *ime_message = output->mutable_ime();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(
    RenderWidgetMessage** render_widget_message,
    int target_tab_id) {
  DCHECK(render_widget_message);
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::RENDER_WIDGET);
  output->set_target_tab_id(target_tab_id);
  *render_widget_message = output->mutable_render_widget();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(SizeMessage** size_message) {
  DCHECK(size_message);
  TabControlMessage* control_message;
  std::unique_ptr<BlimpMessage> output = CreateBlimpMessage(&control_message);
  control_message->set_type(TabControlMessage::SIZE);
  *size_message = control_message->mutable_size();
  return output;
}

std::unique_ptr<BlimpMessage> CreateBlimpMessage(
    EngineSettingsMessage** engine_settings) {
  DCHECK(engine_settings);
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::SETTINGS);
  *engine_settings = output->mutable_settings()->mutable_engine_settings();
  return output;
}

std::unique_ptr<BlimpMessage> CreateStartConnectionMessage(
    const std::string& client_token,
    int protocol_version) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::PROTOCOL_CONTROL);

  ProtocolControlMessage* control_message = output->mutable_protocol_control();
  control_message->set_type(ProtocolControlMessage::START_CONNECTION);

  StartConnectionMessage* start_connection_message =
      control_message->mutable_start_connection();
  start_connection_message->set_client_token(client_token);
  start_connection_message->set_protocol_version(protocol_version);

  return output;
}

std::unique_ptr<BlimpMessage> CreateCheckpointAckMessage(
    int64_t checkpoint_id) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->set_type(BlimpMessage::PROTOCOL_CONTROL);

  ProtocolControlMessage* control_message = output->mutable_protocol_control();
  control_message->set_type(ProtocolControlMessage::CHECKPOINT_ACK);

  CheckpointAckMessage* checkpoint_ack_message =
      control_message->mutable_checkpoint_ack();
  checkpoint_ack_message->set_checkpoint_id(checkpoint_id);

  return output;
}

}  // namespace blimp
