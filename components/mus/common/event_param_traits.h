// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_COMMON_EVENT_PARAM_TRAITS_H_
#define COMPONENTS_MUS_COMMON_EVENT_PARAM_TRAITS_H_

#include <string>

#include "components/mus/common/event_param_traits_macros.h"
#include "components/mus/common/mus_common_export.h"
#include "ui/events/event.h"
#include "ui/events/gesture_event_details.h"

namespace base {
class Pickle;
class PickleIterator;
}

namespace ui {
class Event;
}

namespace IPC {

// Non-serialized data:
//
// Some event data only makes sense and/or is needed in the context where the
// raw event came from. As such, some data are explicitly NOT
// serialized. These data are as follows:
//   base::NativeEvent native_event_;
//   LatencyInfo* latency_;
//   int source_device_id_;

template <>
struct MUS_COMMON_EXPORT ParamTraits<ui::ScopedEvent> {
  typedef ui::ScopedEvent param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);

  static void SizeEvent(ui::EventType type,
                        base::TimeDelta time_stamp,
                        int flags,
                        base::PickleSizer* s,
                        const ui::ScopedEvent& p);
  static void WriteEvent(ui::EventType type,
                         base::TimeDelta time_stamp,
                         int flags,
                         base::Pickle* m,
                         const ui::ScopedEvent& p);
  static bool ReadEvent(ui::EventType type,
                        base::TimeDelta time_stamp,
                        int flags,
                        const base::Pickle* m,
                        base::PickleIterator* iter,
                        ui::ScopedEvent* p);
  static void LogEvent(ui::EventType type,
                       base::TimeDelta time_stamp,
                       int flags,
                       const ui::ScopedEvent& p,
                       std::string* l);
};

// Manually implements no-op implementation for ui::CancelModeEvent because IPC
// BEGIN/END macros with no MEMBER or PARENT in between cause compiler
// errors.
template <>
struct ParamTraits<ui::CancelModeEvent> {
  typedef ui::CancelModeEvent param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ui::GestureEventDetails::Details> {
  typedef ui::GestureEventDetails::Details param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // COMPONENTS_MUS_COMMON_EVENT_PARAM_TRAITS_H_
