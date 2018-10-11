// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/hit_test_debug_key_event_observer.h"

#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "ui/events/keycodes/keyboard_codes.h"

typedef blink::WebInputEvent::Type Type;
typedef blink::WebInputEvent::Modifiers Modifiers;

namespace {

viz::HitTestQuery* GetHitTestQuery(
    viz::HostFrameSinkManager* host_frame_sink_manager,
    const viz::FrameSinkId& frame_sink_id) {
  if (!frame_sink_id.is_valid())
    return nullptr;
  const auto& display_hit_test_query_map =
      host_frame_sink_manager->display_hit_test_query();
  const auto iter = display_hit_test_query_map.find(frame_sink_id);
  if (iter == display_hit_test_query_map.end())
    return nullptr;
  return iter->second.get();
}

}  // namespace

namespace content {

HitTestDebugKeyEventObserver::HitTestDebugKeyEventObserver(
    RenderWidgetHostImpl* host)
    : host_(host), hit_test_query_(nullptr) {
  host_->AddInputEventObserver(this);
}

HitTestDebugKeyEventObserver::~HitTestDebugKeyEventObserver() {
  host_->RemoveInputEventObserver(this);
}

void HitTestDebugKeyEventObserver::OnInputEventAck(
    InputEventAckSource source,
    InputEventAckState state,
    const blink::WebInputEvent& event) {
  if (INPUT_EVENT_ACK_STATE_CONSUMED == state ||
      (event.GetType() != Type::kRawKeyDown &&
       event.GetType() != Type::kKeyDown)) {
    return;
  }

  const blink::WebKeyboardEvent& key_event =
      static_cast<const blink::WebKeyboardEvent&>(event);

  if (key_event.windows_key_code != ui::VKEY_H ||
      key_event.GetModifiers() !=
          (Modifiers::kControlKey | Modifiers::kShiftKey)) {
    return;
  }

  if (!hit_test_query_) {
    hit_test_query_ = GetHitTestQuery(GetHostFrameSinkManager(),
                                      host_->GetView()->GetRootFrameSinkId());
  }
  if (hit_test_query_) {
    std::string printed_hit_test_data = hit_test_query_->PrintHitTestData();
    VLOG(1) << (printed_hit_test_data.empty() ? "No hit-test data."
                                              : printed_hit_test_data);
  }
}

}  // namespace content
