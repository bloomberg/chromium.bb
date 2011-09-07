// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AURA_EVENT_H_
#define AURA_EVENT_H_
#pragma once

#include "aura/aura_export.h"
#include "base/basictypes.h"
#include "base/time.h"
#include "ui/base/events.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/point.h"

#if defined(USE_X11)
typedef union _XEvent XEvent;
#endif

namespace aura {

#if defined(OS_WIN)
typedef MSG NativeEvent;
#elif defined(USE_X11)
typedef XEvent* NativeEvent;
#endif

class Window;

class Event {
 public:
  const NativeEvent& native_event() const { return native_event_; }
  ui::EventType type() const { return type_; }
  const base::Time& time_stamp() const { return time_stamp_; }
  int flags() const { return flags_; }

 protected:
  Event(ui::EventType type, int flags);
  Event(NativeEvent native_event, ui::EventType type, int flags);
  Event(const Event& copy);

 private:
  void operator=(const Event&);

  // Safely initializes the native event members of this class.
  void Init();
  void InitWithNativeEvent(NativeEvent native_event);

  NativeEvent native_event_;
  ui::EventType type_;
  base::Time time_stamp_;
  int flags_;
};

class LocatedEvent : public Event {
 public:
  int x() const { return location_.x(); }
  int y() const { return location_.y(); }
  gfx::Point location() const { return location_; }

 protected:
  explicit LocatedEvent(NativeEvent native_event);

  // Create a new LocatedEvent which is identical to the provided model.
  // If source / target windows are provided, the model location will be
  // converted from |source| coordinate system to |target| coordinate system.
  LocatedEvent(const LocatedEvent& model, Window* source, Window* target);

  // Used for synthetic events in testing.
  LocatedEvent(ui::EventType type, const gfx::Point& location, int flags);

  gfx::Point location_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocatedEvent);
};

class AURA_EXPORT MouseEvent : public LocatedEvent {
 public:
  explicit MouseEvent(NativeEvent native_event);

  // Create a new MouseEvent which is identical to the provided model.
  // If source / target windows are provided, the model location will be
  // converted from |source| coordinate system to |target| coordinate system.
  MouseEvent(const MouseEvent& model, Window* source, Window* target);

  // Used for synthetic events in testing.
  MouseEvent(ui::EventType type, const gfx::Point& location, int flags);

 private:
  DISALLOW_COPY_AND_ASSIGN(MouseEvent);
};

class AURA_EXPORT KeyEvent : public Event {
 public:
  explicit KeyEvent(NativeEvent native_event);

  // Used for synthetic events in testing.
  KeyEvent(ui::EventType type,
           ui::KeyboardCode key_code,
           int flags);

  ui::KeyboardCode key_code() const { return key_code_; }

 private:
  ui::KeyboardCode key_code_;
};

}  // namespace aura

#endif  // AURA_EVENT_H_
