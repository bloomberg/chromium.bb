// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/common/event_param_traits.h"

#include <string.h>

#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"
#include "ui/events/event.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"

// Generate param traits size methods.
#include "ipc/param_traits_size_macros.h"
namespace IPC {
#include "components/mus/common/event_param_traits_macros.h"
}

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "components/mus/common/event_param_traits_macros.h"
}

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "components/mus/common/event_param_traits_macros.h"
}

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "components/mus/common/event_param_traits_macros.h"
}

namespace IPC {

// Implements (Write|Read|Log)Event for event type-qualified functions. Every
// such function invokes an implementation according to the event type and
// flags, or else invokes a default implementation. Event constructors require
// |type|, |time_stamp|, and |flags| (hence the common arguments passed to each
// implementation).
#define EVENT_IMPL(ReturnType, methodName, implName, defaultCase, ...)  \
  ReturnType ParamTraits<ui::ScopedEvent>::methodName(                  \
      ui::EventType type, base::TimeDelta time_stamp, int flags,        \
      ##__VA_ARGS__) {                                                  \
    switch (type) {                                                     \
      case ui::EventType::ET_MOUSE_PRESSED:                             \
      case ui::EventType::ET_MOUSE_DRAGGED:                             \
      case ui::EventType::ET_MOUSE_RELEASED:                            \
      case ui::EventType::ET_MOUSE_MOVED:                               \
      case ui::EventType::ET_MOUSE_ENTERED:                             \
      case ui::EventType::ET_MOUSE_EXITED:                              \
      case ui::EventType::ET_MOUSE_CAPTURE_CHANGED:                     \
        implName(ui::MouseEvent)                                        \
      case ui::EventType::ET_KEY_PRESSED:                               \
      case ui::EventType::ET_KEY_RELEASED:                              \
        implName(ui::KeyEvent)                                          \
      case ui::EventType::ET_MOUSEWHEEL:                                \
        implName(ui::MouseWheelEvent)                                   \
      case ui::EventType::ET_TOUCH_RELEASED:                            \
      case ui::EventType::ET_TOUCH_PRESSED:                             \
      case ui::EventType::ET_TOUCH_MOVED:                               \
      case ui::EventType::ET_TOUCH_CANCELLED:                           \
      case ui::EventType::ET_DROP_TARGET_EVENT:                         \
        implName(ui::TouchEvent)                                        \
      case ui::EventType::ET_POINTER_DOWN:                              \
      case ui::EventType::ET_POINTER_MOVED:                             \
      case ui::EventType::ET_POINTER_UP:                                \
      case ui::EventType::ET_POINTER_CANCELLED:                         \
      case ui::EventType::ET_POINTER_ENTERED:                           \
      case ui::EventType::ET_POINTER_EXITED:                            \
        implName(ui::PointerEvent)                                      \
      case ui::EventType::ET_GESTURE_SCROLL_BEGIN:                      \
      case ui::EventType::ET_GESTURE_SCROLL_END:                        \
      case ui::EventType::ET_GESTURE_SCROLL_UPDATE:                     \
      case ui::EventType::ET_GESTURE_SHOW_PRESS:                        \
      case ui::EventType::ET_GESTURE_WIN8_EDGE_SWIPE:                   \
      case ui::EventType::ET_GESTURE_TAP:                               \
      case ui::EventType::ET_GESTURE_TAP_DOWN:                          \
      case ui::EventType::ET_GESTURE_TAP_CANCEL:                        \
      case ui::EventType::ET_GESTURE_BEGIN:                             \
      case ui::EventType::ET_GESTURE_END:                               \
      case ui::EventType::ET_GESTURE_TWO_FINGER_TAP:                    \
      case ui::EventType::ET_GESTURE_PINCH_BEGIN:                       \
      case ui::EventType::ET_GESTURE_PINCH_END:                         \
      case ui::EventType::ET_GESTURE_PINCH_UPDATE:                      \
      case ui::EventType::ET_GESTURE_LONG_PRESS:                        \
      case ui::EventType::ET_GESTURE_LONG_TAP:                          \
      case ui::EventType::ET_GESTURE_SWIPE:                             \
      case ui::EventType::ET_GESTURE_TAP_UNCONFIRMED:                   \
      case ui::EventType::ET_GESTURE_DOUBLE_TAP:                        \
        implName(ui::GestureEvent)                                      \
      case ui::EventType::ET_SCROLL:                                    \
        implName(ui::ScrollEvent)                                       \
      case ui::EventType::ET_SCROLL_FLING_START:                        \
      case ui::EventType::ET_SCROLL_FLING_CANCEL:                       \
        if (flags & ui::MouseEventFlags::EF_FROM_TOUCH) {               \
          implName(ui::GestureEvent)                                    \
        } else {                                                        \
          implName(ui::MouseEvent)                                      \
        }                                                               \
      case ui::EventType::ET_CANCEL_MODE:                               \
        implName(ui::CancelModeEvent)                                   \
      default:                                                          \
        defaultCase;                                                    \
    }                                                                   \
  }

// Concrete event type (T) implementation procedures: size, write, read, log.
#define SIZE_EVENT(T) ParamTraits<T>::GetSize(s, *static_cast<T*>(p.get()));
#define WRITE_EVENT(T)                     \
  ParamTraits<T>::Write(m, *static_cast<T*>(p.get())); \
  break;
#define READ_EVENT(T)                                         \
  {                                                           \
    std::unique_ptr<T> event(new T(type, time_stamp, flags)); \
    if (!ParamTraits<T>::Read(m, iter, event.get())) {        \
      p->reset();                                             \
      return false;                                           \
    } else {                                                  \
      *p = std::move(event);                                  \
      return true;                                            \
    }                                                         \
  }
#define LOG_EVENT(T) return ParamTraits<T>::Log(*static_cast<T*>(p.get()), l);

// void SizeEvent(ui::EventType type, int flags, base::PickleSizer* s,
//                const ui::ScopedEvent& p) { ... }
EVENT_IMPL(void,
           SizeEvent,
           SIZE_EVENT,
           /* default switch/case: no-op */,
           base::PickleSizer* s,
           const ui::ScopedEvent& p)

// void WriteEvent(ui::EventType type, int flags, base::Pickle* m,
//                 const ui::ScopedEvent& p) { ... }
EVENT_IMPL(void,
           WriteEvent,
           WRITE_EVENT,
           /* default switch/case: no-op */,
           base::Pickle* m,
           const ui::ScopedEvent& p)

// bool ReadEvent(ui::EventType type, int flags, base::Pickle* m,
//                base::PickleIterator* iter, ui::ScopedEvent* p) { ... }
EVENT_IMPL(bool,
           ReadEvent,
           READ_EVENT,
           return false;,
           const base::Pickle* m,
           base::PickleIterator* iter,
           ui::ScopedEvent* p)

// void LogEvent(ui::EventType type, int flags, const ui::ScopedEvent& p,
//               std::string* l) { ... }
EVENT_IMPL(void,
           LogEvent,
           LOG_EVENT,
           /* default switch/case: no-op */,
           const ui::ScopedEvent& p,
           std::string* l)

#undef SIZE_EVENT
#undef WRITE_EVENT
#undef READ_EVENT
#undef LOG_EVENT
#undef EVENT_IMPL

void ParamTraits<ui::ScopedEvent>::GetSize(base::PickleSizer* s,
                                           const param_type& p) {
  DCHECK(p);
  GetParamSize(s, p->type_);
  GetParamSize(s, p->name_);
  GetParamSize(s, p->time_stamp_);
  GetParamSize(s, p->flags_);
  GetParamSize(s, p->phase_);
  GetParamSize(s, p->result_);
  GetParamSize(s, p->cancelable_);
  SizeEvent(p->type_, p->time_stamp_, p->flags_, s, p);
}

void ParamTraits<ui::ScopedEvent>::Write(base::Pickle* m, const param_type& p) {
  DCHECK(p);
  WriteParam(m, p->type_);
  WriteParam(m, p->name_);
  WriteParam(m, p->time_stamp_);
  WriteParam(m, p->flags_);
  WriteParam(m, p->phase_);
  WriteParam(m, p->result_);
  WriteParam(m, p->cancelable_);
  WriteEvent(p->type_, p->time_stamp_, p->flags_, m, p);
}

bool ParamTraits<ui::ScopedEvent>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        param_type* p) {
  // Expect: valid ui::ScopedEvent that does not (yet) point to anything.
  DCHECK(p && !*p);

  ui::EventType type;
  std::string name;
  base::TimeDelta time_stamp;
  int flags;
  ui::EventPhase phase;
  ui::EventResult result;
  bool cancelable;

  // Read initial params, then invoke ReadEvent which will reset() |p| to an
  // instance of the correct concrete event type.
  if (!ReadParam(m, iter, &type) || !ReadParam(m, iter, &name) ||
      !ReadParam(m, iter, &time_stamp) || !ReadParam(m, iter, &flags) ||
      !ReadParam(m, iter, &phase) || !ReadParam(m, iter, &result) ||
      !ReadParam(m, iter, &cancelable) ||
      !ReadEvent(type, time_stamp, flags, m, iter, p))
    return false;

  // Fill in abstract event information.
  (*p)->type_ = type;
  (*p)->name_ = name;
  (*p)->time_stamp_ = time_stamp;
  (*p)->flags_ = flags;
  (*p)->phase_ = phase;
  (*p)->result_ = result;
  (*p)->cancelable_ = cancelable;

  return true;
}

void ParamTraits<ui::ScopedEvent>::Log(const param_type& p, std::string* l) {
  l->append("<UI Event: ");
  LogEvent(p->type_, p->time_stamp_, p->flags_, p, l);
  l->append(">");
}

void ParamTraits<ui::CancelModeEvent>::GetSize(base::PickleSizer* s,
                                               const param_type& p) {}

void ParamTraits<ui::CancelModeEvent>::Write(base::Pickle* m,
                                             const param_type& p) {}

bool ParamTraits<ui::CancelModeEvent>::Read(const base::Pickle* m,
                                            base::PickleIterator* iter,
                                            param_type* p) {
  return true;
}

void ParamTraits<ui::CancelModeEvent>::Log(const param_type& p,
                                           std::string* l) {
  l->append("<ui::CancelModeEvent>");
}

void ParamTraits<ui::GestureEventDetails::Details>::GetSize(
    base::PickleSizer* s,
    const param_type& p) {
  s->AddBytes(sizeof(param_type));
}

void ParamTraits<ui::GestureEventDetails::Details>::Write(base::Pickle* m,
                                                          const param_type& p) {
  m->WriteBytes(&p, sizeof(param_type));
}

bool ParamTraits<ui::GestureEventDetails::Details>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  const char* data;
  if (!iter->ReadBytes(&data, sizeof(param_type)))
    return false;

  memcpy(p, data, sizeof(param_type));
  return true;
}

void ParamTraits<ui::GestureEventDetails::Details>::Log(const param_type& p,
                                                        std::string* l) {
  l->append("<ui::GestureEventDetails::Details>");
}

}  // namespace IPC
