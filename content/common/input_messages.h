// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_MESSAGES_H_
#define CONTENT_COMMON_INPUT_MESSAGES_H_

// IPC messages for input events and other messages that require processing in
// order relative to input events.

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "cc/input/overscroll_behavior.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/input/input_event.h"
#include "content/common/input/input_event_dispatch_type.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_pointer_action_list_params.h"
#include "content/common/input/synthetic_pointer_action_params.h"
#include "content/common/input/synthetic_smooth_drag_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/ipc/ui_events_param_traits_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"
#include "ui/gfx/range/range.h"
#include "ui/latency/ipc/latency_info_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

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
  IPC_STRUCT_TRAITS_MEMBER(fling_velocity_x)
  IPC_STRUCT_TRAITS_MEMBER(fling_velocity_y)
  IPC_STRUCT_TRAITS_MEMBER(granularity)
  IPC_STRUCT_TRAITS_MEMBER(key_modifiers)
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
  IPC_STRUCT_TRAITS_MEMBER(pointer_id_)
  IPC_STRUCT_TRAITS_MEMBER(position_)
  IPC_STRUCT_TRAITS_MEMBER(button_)
  IPC_STRUCT_TRAITS_MEMBER(key_modifiers_)
  IPC_STRUCT_TRAITS_MEMBER(width_)
  IPC_STRUCT_TRAITS_MEMBER(height_)
  IPC_STRUCT_TRAITS_MEMBER(rotation_angle_)
  IPC_STRUCT_TRAITS_MEMBER(force_)
  IPC_STRUCT_TRAITS_MEMBER(timestamp_)
  IPC_STRUCT_TRAITS_MEMBER(duration_)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyntheticPointerActionListParams)
  IPC_STRUCT_TRAITS_PARENT(content::SyntheticGestureParams)
  IPC_STRUCT_TRAITS_MEMBER(params)
IPC_STRUCT_TRAITS_END()

#endif  // CONTENT_COMMON_INPUT_MESSAGES_H_
