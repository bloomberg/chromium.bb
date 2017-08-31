// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for input events and other messages that require processing in
// order relative to input events.
// Multiply-included message file, hence no include guard.

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "cc/input/scroll_boundary_behavior.h"
#include "cc/input/touch_action.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/edit_command.h"
#include "content/common/input/input_event.h"
#include "content/common/input/input_event_ack.h"
#include "content/common/input/input_event_ack_source.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/common/input/input_event_dispatch_type.h"
#include "content/common/input/input_param_traits.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_pointer_action_list_params.h"
#include "content/common/input/synthetic_pointer_action_params.h"
#include "content/common/input/synthetic_smooth_drag_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebPointerProperties.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"
#include "ui/gfx/range/range.h"
#include "ui/latency/ipc/latency_info_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#ifdef IPC_MESSAGE_START
#error IPC_MESSAGE_START
#endif

#define IPC_MESSAGE_START InputMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::InputEventAckSource,
                          content::InputEventAckSource::MAX_FROM_RENDERER)
IPC_ENUM_TRAITS_MAX_VALUE(
    content::SyntheticGestureParams::GestureSourceType,
    content::SyntheticGestureParams::GESTURE_SOURCE_TYPE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(
    content::SyntheticGestureParams::GestureType,
    content::SyntheticGestureParams::SYNTHETIC_GESTURE_TYPE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(
    content::SyntheticPointerActionParams::PointerActionType,
    content::SyntheticPointerActionParams::PointerActionType::
        POINTER_ACTION_TYPE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(
    content::SyntheticPointerActionParams::Button,
    content::SyntheticPointerActionParams::Button::BUTTON_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(content::InputEventDispatchType,
                          content::InputEventDispatchType::DISPATCH_TYPE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(cc::TouchAction, cc::kTouchActionMax)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(blink::WebPointerProperties::Button,
                              blink::WebPointerProperties::Button::kNoButton,
                              blink::WebPointerProperties::Button::kLastEntry)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebPointerProperties::PointerType,
                          blink::WebPointerProperties::PointerType::kLastEntry)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebGestureDevice,
                          (blink::WebGestureDevice::kWebGestureDeviceCount - 1))
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebInputEvent::DispatchType,
                          blink::WebInputEvent::DispatchType::kLastDispatchType)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebGestureEvent::ScrollUnits,
                          blink::WebGestureEvent::ScrollUnits::kLastScrollUnit)
IPC_ENUM_TRAITS_MAX_VALUE(
    blink::WebGestureEvent::InertialPhaseState,
    blink::WebGestureEvent::InertialPhaseState::kLastPhase)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebTouchPoint::State,
                          blink::WebTouchPoint::State::kStateMax)
IPC_ENUM_TRAITS_MAX_VALUE(
    cc::ScrollBoundaryBehavior::ScrollBoundaryBehaviorType,
    cc::ScrollBoundaryBehavior::ScrollBoundaryBehaviorType::
        kScrollBoundaryBehaviorTypeMax)

IPC_STRUCT_TRAITS_BEGIN(ui::DidOverscrollParams)
  IPC_STRUCT_TRAITS_MEMBER(accumulated_overscroll)
  IPC_STRUCT_TRAITS_MEMBER(latest_overscroll_delta)
  IPC_STRUCT_TRAITS_MEMBER(current_fling_velocity)
  IPC_STRUCT_TRAITS_MEMBER(causal_event_viewport_point)
  IPC_STRUCT_TRAITS_MEMBER(scroll_boundary_behavior)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::ScrollBoundaryBehavior)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::EditCommand)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(value)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::InputEvent)
  IPC_STRUCT_TRAITS_MEMBER(web_event)
  IPC_STRUCT_TRAITS_MEMBER(latency_info)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyntheticGestureParams)
  IPC_STRUCT_TRAITS_MEMBER(gesture_source_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyntheticSmoothDragGestureParams)
  IPC_STRUCT_TRAITS_PARENT(content::SyntheticGestureParams)
  IPC_STRUCT_TRAITS_MEMBER(start_point)
  IPC_STRUCT_TRAITS_MEMBER(distances)
  IPC_STRUCT_TRAITS_MEMBER(speed_in_pixels_s)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyntheticSmoothScrollGestureParams)
  IPC_STRUCT_TRAITS_PARENT(content::SyntheticGestureParams)
  IPC_STRUCT_TRAITS_MEMBER(anchor)
  IPC_STRUCT_TRAITS_MEMBER(distances)
  IPC_STRUCT_TRAITS_MEMBER(prevent_fling)
  IPC_STRUCT_TRAITS_MEMBER(speed_in_pixels_s)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyntheticPinchGestureParams)
  IPC_STRUCT_TRAITS_PARENT(content::SyntheticGestureParams)
  IPC_STRUCT_TRAITS_MEMBER(scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(anchor)
  IPC_STRUCT_TRAITS_MEMBER(relative_pointer_speed_in_pixels_s)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyntheticTapGestureParams)
  IPC_STRUCT_TRAITS_PARENT(content::SyntheticGestureParams)
  IPC_STRUCT_TRAITS_MEMBER(position)
  IPC_STRUCT_TRAITS_MEMBER(duration_ms)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyntheticPointerActionParams)
  IPC_STRUCT_TRAITS_MEMBER(pointer_action_type_)
  IPC_STRUCT_TRAITS_MEMBER(index_)
  IPC_STRUCT_TRAITS_MEMBER(position_)
  IPC_STRUCT_TRAITS_MEMBER(button_)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyntheticPointerActionListParams)
  IPC_STRUCT_TRAITS_PARENT(content::SyntheticGestureParams)
  IPC_STRUCT_TRAITS_MEMBER(params)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::InputEventAck)
  IPC_STRUCT_TRAITS_MEMBER(source)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(latency)
  IPC_STRUCT_TRAITS_MEMBER(overscroll)
  IPC_STRUCT_TRAITS_MEMBER(unique_touch_event_id)
  IPC_STRUCT_TRAITS_MEMBER(touch_action)
IPC_STRUCT_TRAITS_END()

// Sends an input event to the render widget. The input event in general
// contains a list of coalesced events and one event that is representative of
// all those events (https://w3c.github.io/pointerevents/extension.html).
IPC_MESSAGE_ROUTED4(
    InputMsg_HandleInputEvent,
    IPC::WebInputEventPointer /* event */,
    std::vector<IPC::WebInputEventPointer> /* coalesced events */,
    ui::LatencyInfo /* latency_info */,
    content::InputEventDispatchType)

// Sends the cursor visibility state to the render widget.
IPC_MESSAGE_ROUTED1(InputMsg_CursorVisibilityChange,
                    bool /* is_visible */)

// Sets the text composition to be between the given start and end offsets in
// the currently focused editable field.
IPC_MESSAGE_ROUTED3(InputMsg_SetCompositionFromExistingText,
                    int /* start */,
                    int /* end */,
                    std::vector<blink::WebImeTextSpan> /* ime_text_spans */)

// Deletes the current selection plus the specified number of characters before
// and after the selection or caret.
IPC_MESSAGE_ROUTED2(InputMsg_ExtendSelectionAndDelete,
                    int /* before */,
                    int /* after */)

// Deletes text before and after the current cursor position, excluding the
// selection. The lengths are supplied in Java chars (UTF-16 Code Unit), not in
// code points or in glyphs.
IPC_MESSAGE_ROUTED2(InputMsg_DeleteSurroundingText,
                    int /* before */,
                    int /* after */)

// Deletes text before and after the current cursor position, excluding the
// selection. The lengths are supplied in code points, not in Java chars (UTF-16
// Code Unit) or in glyphs. Does nothing if there are one or more invalid
// surrogate pairs in the requested range
IPC_MESSAGE_ROUTED2(InputMsg_DeleteSurroundingTextInCodePoints,
                    int /* before */,
                    int /* after */)

// Selects between the given start and end offsets in the currently focused
// editable field.
IPC_MESSAGE_ROUTED2(InputMsg_SetEditableSelectionOffsets,
                    int /* start */,
                    int /* end */)

// This message sends a string being composed with an input method.
IPC_MESSAGE_ROUTED5(InputMsg_ImeSetComposition,
                    base::string16,                     /* text */
                    std::vector<blink::WebImeTextSpan>, /* ime_text_spans */
                    gfx::Range /* replacement_range */,
                    int, /* selectiont_start */
                    int /* selection_end */)

// This message deletes the current composition, inserts specified text, and
// moves the cursor.
IPC_MESSAGE_ROUTED4(InputMsg_ImeCommitText,
                    base::string16 /* text */,
                    std::vector<blink::WebImeTextSpan>, /* ime_text_spans */
                    gfx::Range /* replacement_range */,
                    int /* relative_cursor_pos */)

// This message inserts the ongoing composition.
IPC_MESSAGE_ROUTED1(InputMsg_ImeFinishComposingText, bool /* keep_selection */)

// This message notifies the renderer that the next key event is bound to one
// or more pre-defined edit commands. If the next key event is not handled
// by webkit, the specified edit commands shall be executed against current
// focused frame.
// Parameters
// * edit_commands (see chrome/common/edit_command_types.h)
//   Contains one or more edit commands.
// See third_party/WebKit/Source/WebCore/editing/EditorCommand.cpp for detailed
// definition of webkit edit commands.
//
// This message must be sent just before sending a key event.
IPC_MESSAGE_ROUTED1(InputMsg_SetEditCommandsForNextKeyEvent,
                    std::vector<content::EditCommand> /* edit_commands */)

// Message payload is the name of a WebCore edit command to execute.
IPC_MESSAGE_ROUTED1(InputMsg_ExecuteNoValueEditCommand, std::string /* name */)

IPC_MESSAGE_ROUTED0(InputMsg_MouseCaptureLost)

// TODO(darin): figure out how this meshes with RestoreFocus
IPC_MESSAGE_ROUTED1(InputMsg_SetFocus,
                    bool /* enable */)

// Tells the renderer to scroll the currently focused node into rect only if
// the currently focused node is a Text node (textfield, text area or content
// editable divs).
IPC_MESSAGE_ROUTED1(InputMsg_ScrollFocusedEditableNodeIntoRect, gfx::Rect)

// These messages are typically generated from context menus and request the
// renderer to apply the specified operation to the current selection.
IPC_MESSAGE_ROUTED0(InputMsg_Undo)
IPC_MESSAGE_ROUTED0(InputMsg_Redo)
IPC_MESSAGE_ROUTED0(InputMsg_Cut)
IPC_MESSAGE_ROUTED0(InputMsg_Copy)
#if defined(OS_MACOSX)
IPC_MESSAGE_ROUTED0(InputMsg_CopyToFindPboard)
#endif
IPC_MESSAGE_ROUTED0(InputMsg_Paste)
IPC_MESSAGE_ROUTED0(InputMsg_PasteAndMatchStyle)
// Replaces the selected region or a word around the cursor with the
// specified string.
IPC_MESSAGE_ROUTED1(InputMsg_Replace,
                    base::string16)
// Replaces the misspelling in the selected region with the specified string.
IPC_MESSAGE_ROUTED1(InputMsg_ReplaceMisspelling,
                    base::string16)
IPC_MESSAGE_ROUTED0(InputMsg_Delete)
IPC_MESSAGE_ROUTED0(InputMsg_SelectAll)

IPC_MESSAGE_ROUTED0(InputMsg_CollapseSelection)

// Requests the renderer to select the region between two points.
// Expects a SelectRange_ACK message when finished.
IPC_MESSAGE_ROUTED2(InputMsg_SelectRange,
                    gfx::Point /* base */,
                    gfx::Point /* extent */)

// Sent by the browser to ask the renderer to adjust the selection start and
// end points by the given amounts. A negative amount moves the selection
// towards the beginning of the document, a positive amount moves the selection
// towards the end of the document.
IPC_MESSAGE_ROUTED2(InputMsg_AdjustSelectionByCharacterOffset,
                    int /* start_adjust*/,
                    int /* end_adjust */)

// Requests the renderer to move the selection extent point to a new position.
// Expects a MoveRangeSelectionExtent_ACK message when finished.
IPC_MESSAGE_ROUTED1(InputMsg_MoveRangeSelectionExtent,
                    gfx::Point /* extent */)

// Requests the renderer to move the caret selection toward the point.
// Expects a MoveCaret_ACK message when finished.
IPC_MESSAGE_ROUTED1(InputMsg_MoveCaret,
                    gfx::Point /* location */)

#if defined(OS_ANDROID)
// Request from browser to update text input state.
IPC_MESSAGE_ROUTED0(InputMsg_RequestTextInputStateUpdate)
#endif

// Request from browser to update the cursor and composition information which
// will be sent through InputHostMsg_ImeCompositionRangeChanged. Setting
// |immediate_request| to true  will lead to an immediate update. If
// |monitor_updates| is set to true then changes to text selection or regular
// updates in each compositor frame (when there is a change in composition info)
// will lead to updates being sent to the browser.
IPC_MESSAGE_ROUTED2(InputMsg_RequestCompositionUpdates,
                    bool /* immediate_request */,
                    bool /* monitor_updates */)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Acknowledges receipt of a InputMsg_HandleInputEvent message.
IPC_MESSAGE_ROUTED1(InputHostMsg_HandleInputEvent_ACK,
                    content::InputEventAck /* ack */)

// Notifies the allowed touch actions for a new touch point.
IPC_MESSAGE_ROUTED1(InputHostMsg_SetTouchAction,
                    cc::TouchAction /* touch_action */)

// The whitelisted touch action and the associated unique touch event id
// for a new touch point sent by the compositor. The unique touch event id is
// only needed to verify that the whitelisted touch action is being associated
// with the correct touch event. The input event ack state is needed when
// the touchstart message was not sent to the renderer and the touch
// actions need to be reset and the touch ack timeout needs to be started.
IPC_MESSAGE_ROUTED3(InputHostMsg_SetWhiteListedTouchAction,
                    cc::TouchAction /* white_listed_touch_action */,
                    uint32_t /* unique_touch_event_id */,
                    content::InputEventAckState /* ack_result */)

// Sent by the compositor when input scroll events are dropped due to bounds
// restrictions on the root scroll offset.
IPC_MESSAGE_ROUTED1(InputHostMsg_DidOverscroll,
                    ui::DidOverscrollParams /* params */)

// Sent by the compositor when a fling animation is stopped.
IPC_MESSAGE_ROUTED0(InputHostMsg_DidStopFlinging)

// Acknowledges receipt of a InputMsg_MoveCaret message.
IPC_MESSAGE_ROUTED0(InputHostMsg_MoveCaret_ACK)

// Acknowledges receipt of a InputMsg_MoveRangeSelectionExtent message.
IPC_MESSAGE_ROUTED0(InputHostMsg_MoveRangeSelectionExtent_ACK)

// Acknowledges receipt of a InputMsg_SelectRange message.
IPC_MESSAGE_ROUTED0(InputHostMsg_SelectRange_ACK)

// Required for cancelling an ongoing input method composition.
IPC_MESSAGE_ROUTED0(InputHostMsg_ImeCancelComposition)

// This IPC message sends the character bounds after every composition change
// to always have correct bound info.
IPC_MESSAGE_ROUTED2(InputHostMsg_ImeCompositionRangeChanged,
                    gfx::Range /* composition range */,
                    std::vector<gfx::Rect> /* character bounds */)

// Adding a new message? Stick to the sort order above: first platform
// independent InputMsg, then ifdefs for platform specific InputMsg, then
// platform independent InputHostMsg, then ifdefs for platform specific
// InputHostMsg.
