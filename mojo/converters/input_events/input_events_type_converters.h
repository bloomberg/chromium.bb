// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CONVERTERS_INPUT_EVENTS_INPUT_EVENTS_TYPE_CONVERTERS_H_
#define MOJO_CONVERTERS_INPUT_EVENTS_INPUT_EVENTS_TYPE_CONVERTERS_H_

#include <memory>

#include "components/mus/public/interfaces/input_events.mojom.h"
#include "mojo/converters/input_events/mojo_input_events_export.h"
#include "ui/events/event.h"

namespace mojo {

// NOTE: the mojo input events do not necessarily provide a 1-1 mapping with
// ui::Event types. Be careful in using them!
template <>
struct MOJO_INPUT_EVENTS_EXPORT
    TypeConverter<mus::mojom::EventType, ui::EventType> {
  static mus::mojom::EventType Convert(ui::EventType type);
};

template <>
struct MOJO_INPUT_EVENTS_EXPORT TypeConverter<mus::mojom::EventPtr, ui::Event> {
  static mus::mojom::EventPtr Convert(const ui::Event& input);
};

template <>
struct MOJO_INPUT_EVENTS_EXPORT
    TypeConverter<mus::mojom::EventPtr, ui::KeyEvent> {
  static mus::mojom::EventPtr Convert(const ui::KeyEvent& input);
};

template <>
struct MOJO_INPUT_EVENTS_EXPORT
    TypeConverter<mus::mojom::EventPtr, ui::GestureEvent> {
  static mus::mojom::EventPtr Convert(const ui::GestureEvent& input);
};

template <>
struct MOJO_INPUT_EVENTS_EXPORT
    TypeConverter<std::unique_ptr<ui::Event>, mus::mojom::EventPtr> {
  static std::unique_ptr<ui::Event> Convert(const mus::mojom::EventPtr& input);
};

}  // namespace mojo

#endif  // MOJO_CONVERTERS_INPUT_EVENTS_INPUT_EVENTS_TYPE_CONVERTERS_H_
