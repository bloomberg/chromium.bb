// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/feature/engine_render_widget_feature.h"

#include "base/numerics/safe_conversions.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/input.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "content/public/browser/render_widget_host.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace blimp {

EngineRenderWidgetFeature::EngineRenderWidgetFeature() {}

EngineRenderWidgetFeature::~EngineRenderWidgetFeature() {}

void EngineRenderWidgetFeature::set_render_widget_message_sender(
    scoped_ptr<BlimpMessageProcessor> message_processor) {
  DCHECK(message_processor);
  render_widget_message_sender_ = std::move(message_processor);
}

void EngineRenderWidgetFeature::set_input_message_sender(
    scoped_ptr<BlimpMessageProcessor> message_processor) {
  DCHECK(message_processor);
  input_message_sender_ = std::move(message_processor);
}

void EngineRenderWidgetFeature::set_compositor_message_sender(
    scoped_ptr<BlimpMessageProcessor> message_processor) {
  DCHECK(message_processor);
  compositor_message_sender_ = std::move(message_processor);
}

void EngineRenderWidgetFeature::OnRenderWidgetCreated(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host) {
  DCHECK(render_widget_host);

  int render_widget_id = AddRenderWidget(tab_id, render_widget_host);
  DCHECK_GT(render_widget_id, 0);

  RenderWidgetMessage* render_widget_message;
  scoped_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&render_widget_message, tab_id);
  render_widget_message->set_type(RenderWidgetMessage::CREATED);
  render_widget_message->set_render_widget_id(render_widget_id);

  render_widget_message_sender_->ProcessMessage(std::move(blimp_message),
                                                net::CompletionCallback());
}

void EngineRenderWidgetFeature::OnRenderWidgetInitialized(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host) {
  DCHECK(render_widget_host);

  int render_widget_id = GetRenderWidgetId(tab_id, render_widget_host);
  DCHECK_GT(render_widget_id, 0);

  RenderWidgetMessage* render_widget_message;
  scoped_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&render_widget_message, tab_id);
  render_widget_message->set_type(RenderWidgetMessage::INITIALIZE);
  render_widget_message->set_render_widget_id(render_widget_id);

  render_widget_message_sender_->ProcessMessage(std::move(blimp_message),
                                                net::CompletionCallback());
}

void EngineRenderWidgetFeature::OnRenderWidgetDeleted(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host) {
  DCHECK(render_widget_host);

  int render_widget_id = DeleteRenderWidget(tab_id, render_widget_host);
  DCHECK_GT(render_widget_id, 0);

  RenderWidgetMessage* render_widget_message;
  scoped_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&render_widget_message, tab_id);
  render_widget_message->set_type(RenderWidgetMessage::DELETED);
  render_widget_message->set_render_widget_id(render_widget_id);

  render_widget_message_sender_->ProcessMessage(std::move(blimp_message),
                                                net::CompletionCallback());
}

void EngineRenderWidgetFeature::SendCompositorMessage(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host,
    const std::vector<uint8_t>& message) {
  CompositorMessage* compositor_message;
  scoped_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&compositor_message, tab_id);

  int render_widget_id = GetRenderWidgetId(tab_id, render_widget_host);
  DCHECK_GT(render_widget_id, 0);

  compositor_message->set_render_widget_id(render_widget_id);
  // TODO(dtrainor): Move the transport medium to std::string* and use
  // set_allocated_payload.
  compositor_message->set_payload(message.data(),
                                  base::checked_cast<int>(message.size()));

  compositor_message_sender_->ProcessMessage(std::move(blimp_message),
                                                net::CompletionCallback());
}

void EngineRenderWidgetFeature::SetDelegate(
    const int tab_id,
    RenderWidgetMessageDelegate* delegate) {
  DCHECK(!FindDelegate(tab_id));
  delegates_[tab_id] = delegate;
}

void EngineRenderWidgetFeature::RemoveDelegate(const int tab_id) {
  DelegateMap::iterator it = delegates_.find(tab_id);
  if (it != delegates_.end())
    delegates_.erase(it);
}

void EngineRenderWidgetFeature::ProcessMessage(
    scoped_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(message->type() == BlimpMessage::RENDER_WIDGET ||
         message->type() == BlimpMessage::INPUT ||
         message->type() == BlimpMessage::COMPOSITOR);

  int target_tab_id = message->target_tab_id();

  RenderWidgetMessageDelegate* delegate = FindDelegate(target_tab_id);
  DCHECK(delegate);

  content::RenderWidgetHost* render_widget_host = nullptr;

  switch (message->type()) {
    case BlimpMessage::INPUT:
      render_widget_host = GetRenderWidgetHost(target_tab_id,
                              message->input().render_widget_id());
      if (render_widget_host) {
        scoped_ptr<blink::WebGestureEvent> event =
            input_message_converter_.ProcessMessage(message->input());
        if (event)
          delegate->OnWebGestureEvent(render_widget_host, std::move(event));
      }
      break;
    case BlimpMessage::COMPOSITOR:
      render_widget_host = GetRenderWidgetHost(target_tab_id,
                                    message->compositor().render_widget_id());
      if (render_widget_host) {
        std::vector<uint8_t> payload(message->compositor().payload().size());
        memcpy(payload.data(),
               message->compositor().payload().data(),
               payload.size());
        delegate->OnCompositorMessageReceived(render_widget_host, payload);
      }
      break;
    default:
      NOTREACHED();
  }

  callback.Run(net::OK);
}

EngineRenderWidgetFeature::RenderWidgetMessageDelegate*
EngineRenderWidgetFeature::FindDelegate(const int tab_id) {
  DelegateMap::const_iterator it = delegates_.find(tab_id);
  if (it != delegates_.end())
    return it->second;
  return nullptr;
}

int EngineRenderWidgetFeature::AddRenderWidget(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host) {
  TabMap::iterator tab_it = tabs_.find(tab_id);
  if (tab_it == tabs_.end()) {
    tabs_[tab_id] = std::make_pair(RenderWidgetToIdMap(),
                                   IdToRenderWidgetMap());
    tab_it = tabs_.find(tab_id);
  }

  int render_widget_id = next_widget_id_.GetNext() + 1;

  RenderWidgetMaps* render_widget_maps = &tab_it->second;

  RenderWidgetToIdMap* render_widget_to_id = &render_widget_maps->first;
  IdToRenderWidgetMap* id_to_render_widget = &render_widget_maps->second;

  DCHECK(render_widget_to_id->find(render_widget_host) ==
      render_widget_to_id->end());
  DCHECK(id_to_render_widget->find(render_widget_id) ==
      id_to_render_widget->end());

  (*render_widget_to_id)[render_widget_host] = render_widget_id;
  (*id_to_render_widget)[render_widget_id] = render_widget_host;

  return render_widget_id;
}

int EngineRenderWidgetFeature::DeleteRenderWidget(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host) {
  TabMap::iterator tab_it = tabs_.find(tab_id);
  DCHECK(tab_it != tabs_.end());

  RenderWidgetMaps* render_widget_maps = &tab_it->second;

  RenderWidgetToIdMap* render_widget_to_id = &render_widget_maps->first;
  RenderWidgetToIdMap::iterator widget_to_id_it =
      render_widget_to_id->find(render_widget_host);
  DCHECK(widget_to_id_it != render_widget_to_id->end());

  int render_widget_id = widget_to_id_it->second;
  render_widget_to_id->erase(widget_to_id_it);

  IdToRenderWidgetMap* id_to_render_widget = &render_widget_maps->second;
  IdToRenderWidgetMap::iterator id_to_widget_it =
      id_to_render_widget->find(render_widget_id);
  DCHECK(id_to_widget_it->second == render_widget_host);
  id_to_render_widget->erase(id_to_widget_it);

  return render_widget_id;
}

int EngineRenderWidgetFeature::GetRenderWidgetId(
    const int tab_id,
    content::RenderWidgetHost* render_widget_host) {
  TabMap::const_iterator tab_it = tabs_.find(tab_id);
  if (tab_it == tabs_.end())
    return 0;

  const RenderWidgetMaps* render_widget_maps = &tab_it->second;
  const RenderWidgetToIdMap* render_widget_to_id = &render_widget_maps->first;

  RenderWidgetToIdMap::const_iterator widget_it =
      render_widget_to_id->find(render_widget_host);
  if (widget_it == render_widget_to_id->end())
    return 0;

  return widget_it->second;
}

content::RenderWidgetHost* EngineRenderWidgetFeature::GetRenderWidgetHost(
    const int tab_id,
    const int render_widget_id) {
  TabMap::const_iterator tab_it = tabs_.find(tab_id);
  if (tab_it == tabs_.end())
    return nullptr;

  const RenderWidgetMaps* render_widget_maps = &tab_it->second;
  const IdToRenderWidgetMap* id_to_render_widget = &render_widget_maps->second;

  IdToRenderWidgetMap::const_iterator widget_id_it =
      id_to_render_widget->find(render_widget_id);
  if (widget_id_it == id_to_render_widget->end())
    return nullptr;

  return widget_id_it->second;
}

}  // namespace blimp
