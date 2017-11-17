// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_
#define CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_

// IPC messages for accessibility.

#include "content/common/ax_content_node_data.h"
#include "content/common/content_export.h"
#include "content/common/view_message_enums.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/param_traits_macros.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/transform.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START AccessibilityMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::AXContentIntAttribute,
                          content::AX_CONTENT_INT_ATTRIBUTE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ui::AXAction, ui::AX_ACTION_LAST)

IPC_STRUCT_TRAITS_BEGIN(ui::AXActionData)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(target_node_id)
  IPC_STRUCT_TRAITS_MEMBER(flags)
  IPC_STRUCT_TRAITS_MEMBER(anchor_node_id)
  IPC_STRUCT_TRAITS_MEMBER(anchor_offset)
  IPC_STRUCT_TRAITS_MEMBER(focus_node_id)
  IPC_STRUCT_TRAITS_MEMBER(focus_offset)
  IPC_STRUCT_TRAITS_MEMBER(target_rect)
  IPC_STRUCT_TRAITS_MEMBER(target_point)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(hit_test_event_to_fire)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::AXContentNodeData)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(role)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(actions)
  IPC_STRUCT_TRAITS_MEMBER(location)
  IPC_STRUCT_TRAITS_MEMBER(transform)
  IPC_STRUCT_TRAITS_MEMBER(string_attributes)
  IPC_STRUCT_TRAITS_MEMBER(int_attributes)
  IPC_STRUCT_TRAITS_MEMBER(float_attributes)
  IPC_STRUCT_TRAITS_MEMBER(bool_attributes)
  IPC_STRUCT_TRAITS_MEMBER(intlist_attributes)
  IPC_STRUCT_TRAITS_MEMBER(html_attributes)
  IPC_STRUCT_TRAITS_MEMBER(child_ids)
  IPC_STRUCT_TRAITS_MEMBER(content_int_attributes)
  IPC_STRUCT_TRAITS_MEMBER(offset_container_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::AXContentTreeData)
  IPC_STRUCT_TRAITS_MEMBER(tree_id)
  IPC_STRUCT_TRAITS_MEMBER(parent_tree_id)
  IPC_STRUCT_TRAITS_MEMBER(focused_tree_id)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(mimetype)
  IPC_STRUCT_TRAITS_MEMBER(doctype)
  IPC_STRUCT_TRAITS_MEMBER(loaded)
  IPC_STRUCT_TRAITS_MEMBER(loading_progress)
  IPC_STRUCT_TRAITS_MEMBER(focus_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_object_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_offset)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_affinity)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_object_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_offset)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_affinity)
  IPC_STRUCT_TRAITS_MEMBER(routing_id)
  IPC_STRUCT_TRAITS_MEMBER(parent_routing_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::AXContentTreeUpdate)
  IPC_STRUCT_TRAITS_MEMBER(has_tree_data)
  IPC_STRUCT_TRAITS_MEMBER(tree_data)
  IPC_STRUCT_TRAITS_MEMBER(node_id_to_clear)
  IPC_STRUCT_TRAITS_MEMBER(root_id)
  IPC_STRUCT_TRAITS_MEMBER(nodes)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(AccessibilityHostMsg_EventParams)
  // The tree update.
  IPC_STRUCT_MEMBER(content::AXContentTreeUpdate, update)

  // Type of event.
  IPC_STRUCT_MEMBER(ui::AXEvent, event_type)

  // ID of the node that the event applies to.
  IPC_STRUCT_MEMBER(int, id)

  // The source of this event.
  IPC_STRUCT_MEMBER(ui::AXEventFrom, event_from)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(AccessibilityHostMsg_LocationChangeParams)
  // ID of the object whose location is changing.
  IPC_STRUCT_MEMBER(int, id)

  // The object's new location info.
  IPC_STRUCT_MEMBER(ui::AXRelativeBounds, new_location)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(AccessibilityHostMsg_FindInPageResultParams)
  // The find in page request id.
  IPC_STRUCT_MEMBER(int, request_id)

  // The index of the result match.
  IPC_STRUCT_MEMBER(int, match_index)

  // The id of the accessibility object for the start of the match range.
  IPC_STRUCT_MEMBER(int, start_id)

  // The character offset into the text of the start object.
  IPC_STRUCT_MEMBER(int, start_offset)

  // The id of the accessibility object for the end of the match range.
  IPC_STRUCT_MEMBER(int, end_id)

  // The character offset into the text of the end object.
  IPC_STRUCT_MEMBER(int, end_offset)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

// Relay a request from assistive technology to perform an action,
// such as focusing or clicking on a node.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_PerformAction,
                    ui::AXActionData  /* action parameters */)

// Determine the accessibility object under a given point.
//
// If the target is an object with a child frame (like if the hit test
// result is an iframe element), it responds with
// AccessibilityHostMsg_ChildFrameHitTestResult so that the
// hit test can be performed recursively on the child frame. Otherwise
// it fires an accessibility event of type |event_to_fire| on the target.
IPC_MESSAGE_ROUTED2(AccessibilityMsg_HitTest,
                    gfx::Point /* location to test */,
                    ui::AXEvent /* event to fire */)

// Tells the render view that a AccessibilityHostMsg_Events
// message was processed and it can send additional events. The argument
// must be the same as the ack_token passed to AccessibilityHostMsg_Events.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_Events_ACK,
                    int /* ack_token */)

// Tell the renderer to reset and send a new accessibility tree from
// scratch because the browser is out of sync. It passes a sequential
// reset token. This should be rare, and if we need reset the same renderer
// too many times we just kill it. After sending a reset, the browser ignores
// incoming accessibility IPCs until it receives one with the matching reset
// token. Conversely, it ignores IPCs with a reset token if it was not
// expecting a reset.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_Reset,
                    int /* reset token */)

// Kill the renderer because we got a fatal error in the accessibility tree
// and we've already reset too many times.
IPC_MESSAGE_ROUTED0(AccessibilityMsg_FatalError)

// Request a one-time snapshot of the accessibility tree without
// enabling accessibility if it wasn't already enabled. The passed id
// will be returned in the AccessibilityHostMsg_SnapshotResponse message.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_SnapshotTree,
                    int /* callback id */)

// Messages sent from the renderer to the browser.

// Sent to notify the browser about renderer accessibility events.
// The browser responds with a AccessibilityMsg_Events_ACK with the same
// ack_token.
// The second parameter, reset_token, is set if this IPC was sent in response
// to a reset request from the browser. When the browser requests a reset,
// it ignores incoming IPCs until it sees one with the correct reset token.
// Any other time, it ignores IPCs with a reset token.
IPC_MESSAGE_ROUTED3(
    AccessibilityHostMsg_Events,
    std::vector<AccessibilityHostMsg_EventParams> /* events */,
    int /* reset_token */,
    int /* ack_token */)

// Sent to update the browser of the location of accessibility objects.
IPC_MESSAGE_ROUTED1(
    AccessibilityHostMsg_LocationChanges,
    std::vector<AccessibilityHostMsg_LocationChangeParams>)

// Sent to update the browser of Find In Page results.
IPC_MESSAGE_ROUTED1(
    AccessibilityHostMsg_FindInPageResult,
    AccessibilityHostMsg_FindInPageResultParams)

// Sent in response to AccessibilityMsg_HitTest.
IPC_MESSAGE_ROUTED4(AccessibilityHostMsg_ChildFrameHitTestResult,
                    gfx::Point /* location tested */,
                    int /* routing id of child frame */,
                    int /* browser plugin instance id of child frame */,
                    ui::AXEvent /* event to fire */)

// Sent in response to AccessibilityMsg_SnapshotTree. The callback id that was
// passed to the request will be returned in |callback_id|, along with
// a standalone snapshot of the accessibility tree.
IPC_MESSAGE_ROUTED2(AccessibilityHostMsg_SnapshotResponse,
                    int /* callback_id */,
                    content::AXContentTreeUpdate)

#endif  // CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_
