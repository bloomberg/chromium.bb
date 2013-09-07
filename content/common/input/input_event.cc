// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_event.h"

#include "base/logging.h"

namespace content {

InputEvent::InputEvent() : id_(0) {}

InputEvent::~InputEvent() {}

scoped_ptr<InputEvent> InputEvent::Create(int64 id,
                                          scoped_ptr<Payload> payload) {
  scoped_ptr<InputEvent> event(new InputEvent());
  event->Initialize(id, payload.Pass());
  return event.Pass();
}

bool InputEvent::Initialize(int64 id, scoped_ptr<Payload> payload) {
  id_ = id;
  payload_ = payload.Pass();
  return valid();
}

}  // namespace content
