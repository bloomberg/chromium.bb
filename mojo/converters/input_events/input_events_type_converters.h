// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CONVERTERS_INPUT_EVENTS_INPUT_EVENTS_TYPE_CONVERTERS_H_
#define MOJO_CONVERTERS_INPUT_EVENTS_INPUT_EVENTS_TYPE_CONVERTERS_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/converters/input_events/mojo_input_events_export.h"
#include "ui/events/event.h"
#include "ui/mojo/events/input_events.mojom.h"

namespace mojo {

// NOTE: the mojo input events do not necessarily provide a 1-1 mapping with
// ui::Event types. Be careful in using them!
template <>
struct MOJO_INPUT_EVENTS_EXPORT TypeConverter<EventType, ui::EventType> {
  static EventType Convert(ui::EventType type);
};

template <>
struct MOJO_INPUT_EVENTS_EXPORT TypeConverter<EventPtr, ui::Event> {
  static EventPtr Convert(const ui::Event& input);
};

template <>
struct MOJO_INPUT_EVENTS_EXPORT TypeConverter<EventPtr, ui::KeyEvent> {
  static EventPtr Convert(const ui::KeyEvent& input);
};

template <>
struct MOJO_INPUT_EVENTS_EXPORT TypeConverter<EventPtr, ui::GestureEvent> {
  static EventPtr Convert(const ui::GestureEvent& input);
};

template <>
struct MOJO_INPUT_EVENTS_EXPORT TypeConverter<scoped_ptr<ui::Event>, EventPtr> {
  static scoped_ptr<ui::Event> Convert(const EventPtr& input);
};

}  // namespace mojo

#endif  // MOJO_CONVERTERS_INPUT_EVENTS_INPUT_EVENTS_TYPE_CONVERTERS_H_
