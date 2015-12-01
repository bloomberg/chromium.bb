// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser/engine_render_widget_message_processor.h"

#include "base/numerics/safe_conversions.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/input.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace blimp {

EngineRenderWidgetMessageProcessor::EngineRenderWidgetMessageProcessor(
    BlimpMessageProcessor* render_widget_message_processor,
    BlimpMessageProcessor* compositor_message_processor)
    : render_widget_message_processor_(render_widget_message_processor),
      compositor_message_processor_(compositor_message_processor) {
  // TODO(dtrainor): Register as a BlimpMessageProcessor for
  // BlimpMessage::INPUT and BlimpMessage::COMPOSITOR messages.
}

EngineRenderWidgetMessageProcessor::~EngineRenderWidgetMessageProcessor() {
  // TODO(dtrainor): Unregister as a BlimpMessageProcessor for
  // BlimpMessage::INPUT and BlimpMessage::COMPOSITOR messages.
}

void EngineRenderWidgetMessageProcessor::OnRenderWidgetInitialized(
    const int tab_id) {
  render_widget_ids_[tab_id] = GetRenderWidgetId(tab_id) + 1;

  scoped_ptr<BlimpMessage> blimp_message(new BlimpMessage);
  blimp_message->set_target_tab_id(tab_id);

  RenderWidgetMessage* render_widget_message =
      blimp_message->mutable_render_widget();

  render_widget_message->set_type(RenderWidgetMessage::INITIALIZE);
  render_widget_message->set_render_widget_id(GetRenderWidgetId(tab_id));

  render_widget_message_processor_->ProcessMessage(std::move(blimp_message),
                                                   net::CompletionCallback());
}

void EngineRenderWidgetMessageProcessor::SendCompositorMessage(
    const int tab_id,
    const std::vector<uint8_t>& message) {
  scoped_ptr<BlimpMessage> blimp_message(new BlimpMessage);
  blimp_message->set_target_tab_id(tab_id);

  CompositorMessage* compositor_message = blimp_message->mutable_compositor();
  uint32_t render_widget_id = GetRenderWidgetId(tab_id);
  DCHECK_LT(0U, render_widget_id);
  compositor_message->set_render_widget_id(render_widget_id);
  // TODO(dtrainor): Move the transport medium to std::string* and use
  // set_allocated_payload.
  compositor_message->set_payload(message.data(),
                                  base::checked_cast<int>(message.size()));

  render_widget_message_processor_->ProcessMessage(std::move(blimp_message),
                                                   net::CompletionCallback());
}

void EngineRenderWidgetMessageProcessor::SetDelegate(
    const int tab_id, RenderWidgetMessageDelegate* delegate) {
  DCHECK(!FindDelegate(tab_id));
  delegates_[tab_id] = delegate;
}

void EngineRenderWidgetMessageProcessor::RemoveDelegate(const int tab_id) {
  DelegateMap::iterator it = delegates_.find(tab_id);
  if (it != delegates_.end())
    delegates_.erase(it);
}

void EngineRenderWidgetMessageProcessor::ProcessMessage(
    scoped_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK(message->type() == BlimpMessage::INPUT ||
         message->type() == BlimpMessage::COMPOSITOR);

  int target_tab_id = message->target_tab_id();
  uint32_t render_widget_id = GetRenderWidgetId(target_tab_id);
  DCHECK_GT(render_widget_id, 0U);

  RenderWidgetMessageDelegate* delegate = FindDelegate(target_tab_id);
  DCHECK(delegate);

  switch (message->type()) {
    case BlimpMessage::INPUT:
      if (message->input().render_widget_id() == render_widget_id) {
        scoped_ptr<blink::WebInputEvent> event =
            input_message_processor_.ProcessMessage(message->input());
        if (event)
          delegate->OnWebInputEvent(std::move(event));
      }
      break;
    case BlimpMessage::COMPOSITOR:
      if (message->compositor().render_widget_id() == render_widget_id) {
        std::vector<uint8_t> payload(message->compositor().payload().size());
        memcpy(payload.data(),
               message->compositor().payload().data(),
               payload.size());
        delegate->OnCompositorMessageReceived(payload);
      }
      break;
    default:
      NOTREACHED();
  }

  if (!callback.is_null()) {
    callback.Run(net::OK);
  }
}

EngineRenderWidgetMessageProcessor::RenderWidgetMessageDelegate*
EngineRenderWidgetMessageProcessor::FindDelegate(
    const int tab_id) {
  DelegateMap::const_iterator it = delegates_.find(tab_id);
  if (it != delegates_.end())
    return it->second;
  return nullptr;
}

uint32_t EngineRenderWidgetMessageProcessor::GetRenderWidgetId(
    const int tab_id) {
  RenderWidgetIdMap::const_iterator it = render_widget_ids_.find(tab_id);
  if (it != render_widget_ids_.end())
    return it->second;
  return 0U;
}

}  // namespace blimp
