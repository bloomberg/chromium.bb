// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_INPUT_EVENTS_LIB_MOJO_EXTENDED_KEY_EVENT_DATA_H_
#define MOJO_SERVICES_PUBLIC_CPP_INPUT_EVENTS_LIB_MOJO_EXTENDED_KEY_EVENT_DATA_H_

#include "ui/events/event.h"
#include "mojo/services/public/cpp/input_events/mojo_input_events_export.h"

namespace mojo {

// A structure to store all mojo specific data on a KeyEvent.
class MOJO_INPUT_EVENTS_EXPORT MojoExtendedKeyEventData
    : public ui::ExtendedKeyEventData {
 public:
  MojoExtendedKeyEventData(int32_t windows_key_code,
                           uint16_t text,
                           uint16_t unmodified_text);
  virtual ~MojoExtendedKeyEventData();

  int32_t windows_key_code() const { return windows_key_code_; }
  uint16_t text() const { return text_; }
  uint16_t unmodified_text() const { return unmodified_text_; }

  // ui::ExtendedKeyEventData:
  virtual ui::ExtendedKeyEventData* Clone() const OVERRIDE;

 private:
  const int32_t windows_key_code_;
  const uint16_t text_;
  const uint16_t unmodified_text_;

  DISALLOW_COPY_AND_ASSIGN(MojoExtendedKeyEventData);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_INPUT_EVENTS_LIB_MOJO_EXTENDED_KEY_EVENT_DATA_H_
