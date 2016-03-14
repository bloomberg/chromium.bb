// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "components/mus/common/mus_common_export.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT MUS_COMMON_EXPORT

IPC_ENUM_TRAITS_MIN_MAX_VALUE(ui::EventType,
                              ui::EventType::ET_UNKNOWN,
                              ui::EventType::ET_LAST)

IPC_ENUM_TRAITS_MIN_MAX_VALUE(ui::EventResult,
                              ui::EventResult::ER_UNHANDLED,
                              ui::EventResult::ER_DISABLE_SYNC_HANDLING)

IPC_ENUM_TRAITS_MIN_MAX_VALUE(ui::EventPhase,
                              ui::EventPhase::EP_PREDISPATCH,
                              ui::EventPhase::EP_POSTDISPATCH)

IPC_ENUM_TRAITS_MIN_MAX_VALUE(ui::EventPointerType,
                              ui::EventPhase::EP_PREDISPATCH,
                              ui::EventPhase::EP_POSTDISPATCH)

IPC_ENUM_TRAITS_MIN_MAX_VALUE(ui::KeyboardCode,
                              ui::KeyboardCode::VKEY_UNKNOWN, /* 0x00 */
                              ui::KeyboardCode::VKEY_OEM_CLEAR /* 0xFE */)

IPC_ENUM_TRAITS_MIN_MAX_VALUE(ui::DomCode, 0, 0x0c028c)

IPC_STRUCT_TRAITS_BEGIN(ui::PointerDetails)
  IPC_STRUCT_TRAITS_MEMBER(pointer_type)
  IPC_STRUCT_TRAITS_MEMBER(radius_x)
  IPC_STRUCT_TRAITS_MEMBER(radius_y)
  IPC_STRUCT_TRAITS_MEMBER(force)
  IPC_STRUCT_TRAITS_MEMBER(tilt_x)
  IPC_STRUCT_TRAITS_MEMBER(tilt_y)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::LocatedEvent)
  IPC_STRUCT_TRAITS_MEMBER(location_)
  IPC_STRUCT_TRAITS_MEMBER(root_location_)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::MouseEvent)
  IPC_STRUCT_TRAITS_PARENT(ui::LocatedEvent)
  IPC_STRUCT_TRAITS_MEMBER(changed_button_flags_)
  IPC_STRUCT_TRAITS_MEMBER(pointer_details_)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::MouseWheelEvent)
  IPC_STRUCT_TRAITS_PARENT(ui::MouseEvent)
  IPC_STRUCT_TRAITS_MEMBER(offset_)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::TouchEvent)
  IPC_STRUCT_TRAITS_PARENT(ui::LocatedEvent)
  IPC_STRUCT_TRAITS_MEMBER(touch_id_)
  IPC_STRUCT_TRAITS_MEMBER(unique_event_id_)
  IPC_STRUCT_TRAITS_MEMBER(rotation_angle_)
  IPC_STRUCT_TRAITS_MEMBER(may_cause_scrolling_)
  IPC_STRUCT_TRAITS_MEMBER(pointer_details_)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::PointerEvent)
  IPC_STRUCT_TRAITS_PARENT(ui::LocatedEvent)
  IPC_STRUCT_TRAITS_MEMBER(pointer_id_)
  IPC_STRUCT_TRAITS_MEMBER(details_)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::DomKey)
  IPC_STRUCT_TRAITS_MEMBER(value_)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::KeyEvent)
  IPC_STRUCT_TRAITS_MEMBER(is_char_)
  IPC_STRUCT_TRAITS_MEMBER(key_code_)
  IPC_STRUCT_TRAITS_MEMBER(code_)
  IPC_STRUCT_TRAITS_MEMBER(key_)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::ScrollEvent)
  IPC_STRUCT_TRAITS_PARENT(ui::MouseEvent)
  IPC_STRUCT_TRAITS_MEMBER(x_offset_)
  IPC_STRUCT_TRAITS_MEMBER(y_offset_)
  IPC_STRUCT_TRAITS_MEMBER(x_offset_ordinal_)
  IPC_STRUCT_TRAITS_MEMBER(y_offset_ordinal_)
  IPC_STRUCT_TRAITS_MEMBER(finger_count_)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::GestureEventDetails)
  IPC_STRUCT_TRAITS_MEMBER(type_)
  IPC_STRUCT_TRAITS_MEMBER(data_)
  IPC_STRUCT_TRAITS_MEMBER(touch_points_)
  IPC_STRUCT_TRAITS_MEMBER(bounding_box_)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::GestureEvent)
  IPC_STRUCT_TRAITS_PARENT(ui::LocatedEvent)
  IPC_STRUCT_TRAITS_MEMBER(details_)
IPC_STRUCT_TRAITS_END()
