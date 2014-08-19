// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/automation_internal/automation_util.h"

#include <string>
#include <utility>

#include "base/values.h"
#include "chrome/common/extensions/api/automation_internal.h"
#include "extensions/browser/event_router.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node_data.h"

namespace extensions {

namespace {

void PopulateNodeData(const ui::AXNodeData& node_data,
    linked_ptr< api::automation_internal::AXNodeData>& out_node_data) {
  out_node_data->id = node_data.id;
  out_node_data->role = ToString(node_data.role);

  uint32 state_pos = 0, state_shifter = node_data.state;
  while (state_shifter) {
    if (state_shifter & 1) {
      out_node_data->state.additional_properties.SetBoolean(
          ToString(static_cast<ui::AXState>(state_pos)), true);
    }
    state_shifter = state_shifter >> 1;
    state_pos++;
  }

  out_node_data->location.left = node_data.location.x();
  out_node_data->location.top = node_data.location.y();
  out_node_data->location.width = node_data.location.width();
  out_node_data->location.height = node_data.location.height();

  if (!node_data.bool_attributes.empty()) {
    out_node_data->bool_attributes.reset(
        new api::automation_internal::AXNodeData::BoolAttributes());
    for (size_t i = 0; i < node_data.bool_attributes.size(); ++i) {
      std::pair<ui::AXBoolAttribute, bool> attr =
          node_data.bool_attributes[i];
      out_node_data->bool_attributes->additional_properties.SetBoolean(
          ToString(attr.first), attr.second);
    }
  }

  if (!node_data.float_attributes.empty()) {
    out_node_data->float_attributes.reset(
        new api::automation_internal::AXNodeData::FloatAttributes());
    for (size_t i = 0; i < node_data.float_attributes.size(); ++i) {
      std::pair<ui::AXFloatAttribute, float> attr =
          node_data.float_attributes[i];
      out_node_data->float_attributes->additional_properties.SetDouble(
          ToString(attr.first), attr.second);
    }
  }

  if (!node_data.html_attributes.empty()) {
    out_node_data->html_attributes.reset(
        new api::automation_internal::AXNodeData::HtmlAttributes());
    for (size_t i = 0; i < node_data.html_attributes.size(); ++i) {
      std::pair<std::string, std::string> attr = node_data.html_attributes[i];
      out_node_data->html_attributes->additional_properties.SetString(
          attr.first, attr.second);
    }
  }

  if (!node_data.int_attributes.empty()) {
    out_node_data->int_attributes.reset(
        new api::automation_internal::AXNodeData::IntAttributes());
    for (size_t i = 0; i < node_data.int_attributes.size(); ++i) {
      std::pair<ui::AXIntAttribute, int> attr = node_data.int_attributes[i];
      out_node_data->int_attributes->additional_properties.SetInteger(
          ToString(attr.first), attr.second);
    }
  }

  if (!node_data.intlist_attributes.empty()) {
    out_node_data->intlist_attributes.reset(
        new api::automation_internal::AXNodeData::IntlistAttributes());
    for (size_t i = 0; i < node_data.intlist_attributes.size(); ++i) {
      std::pair<ui::AXIntListAttribute, std::vector<int32> > attr =
          node_data.intlist_attributes[i];
      base::ListValue* intlist = new base::ListValue();
      for (size_t j = 0; j < attr.second.size(); ++j)
        intlist->AppendInteger(attr.second[j]);
      out_node_data->intlist_attributes->additional_properties.Set(
          ToString(attr.first), intlist);
    }
  }

  if (!node_data.string_attributes.empty()) {
    out_node_data->string_attributes.reset(
        new api::automation_internal::AXNodeData::StringAttributes());
    for (size_t i = 0; i < node_data.string_attributes.size(); ++i) {
      std::pair<ui::AXStringAttribute, std::string> attr =
          node_data.string_attributes[i];
      out_node_data->string_attributes->additional_properties.SetString(
          ToString(attr.first), attr.second);
    }
  }

  for (size_t i = 0; i < node_data.child_ids.size(); ++i) {
    out_node_data->child_ids.push_back(node_data.child_ids[i]);
  }
}

void DispatchEventInternal(content::BrowserContext* context,
                           const std::string& event_name,
                           scoped_ptr<base::ListValue> args) {
  if (context && EventRouter::Get(context)) {
    scoped_ptr<Event> event(new Event(event_name, args.Pass()));
    event->restrict_to_browser_context = context;
    EventRouter::Get(context)->BroadcastEvent(event.Pass());
  }
}

}  // namespace

namespace automation_util {

void DispatchAccessibilityEventsToAutomation(
    const std::vector<content::AXEventNotificationDetails>& details,
    content::BrowserContext* browser_context) {
  using api::automation_internal::AXEventParams;
  using api::automation_internal::AXTreeUpdate;

  std::vector<content::AXEventNotificationDetails>::const_iterator iter =
      details.begin();
  for (; iter != details.end(); ++iter) {
    const content::AXEventNotificationDetails& event = *iter;
    AXEventParams ax_event_params;
    ax_event_params.process_id = event.process_id;
    ax_event_params.routing_id = event.routing_id;
    ax_event_params.event_type = ToString(iter->event_type);
    ax_event_params.target_id = event.id;

    AXTreeUpdate& ax_tree_update = ax_event_params.update;
    ax_tree_update.node_id_to_clear = event.node_id_to_clear;
    for (size_t i = 0; i < event.nodes.size(); ++i) {
      linked_ptr<api::automation_internal::AXNodeData> out_node(
          new api::automation_internal::AXNodeData());
      PopulateNodeData(event.nodes[i], out_node);
      ax_tree_update.nodes.push_back(out_node);
    }

    DLOG(INFO) << "AccessibilityEventReceived { "
               << "process_id: " << event.process_id
               << ", routing_id:" << event.routing_id
               << ", event_type:" << ui::ToString(event.event_type)
               << "}; dispatching to JS";

    // TODO(dtseng/aboxhall): Why are we sending only one update at a time? We
    // should match the behavior from renderer -> browser and send a
    // collection of tree updates over (to the extension); see
    // |AccessibilityHostMsg_EventParams| and |AccessibilityHostMsg_Events|.
    DispatchEventInternal(
        browser_context,
        api::automation_internal::OnAccessibilityEvent::kEventName,
        api::automation_internal::OnAccessibilityEvent::Create(
            ax_event_params));
  }
}

void DispatchTreeDestroyedEventToAutomation(
    int process_id,
    int routing_id,
    content::BrowserContext* browser_context) {
  DispatchEventInternal(
      browser_context,
      api::automation_internal::OnAccessibilityTreeDestroyed::kEventName,
      api::automation_internal::OnAccessibilityTreeDestroyed::Create(
          process_id, routing_id));
}

}  // namespace automation_util

}  // namespace extensions
