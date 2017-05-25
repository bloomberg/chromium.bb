// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/events/protocol_event.h"

#include <utility>

#include "base/memory/ptr_util.h"

namespace syncer {

ProtocolEvent::ProtocolEvent() {}

ProtocolEvent::~ProtocolEvent() {}

std::unique_ptr<base::DictionaryValue> ProtocolEvent::ToValue(
    const ProtocolEvent& event) {
  auto dict = base::MakeUnique<base::DictionaryValue>();
  dict->SetDouble("time", event.GetTimestamp().ToJsTime());
  dict->SetString("type", event.GetType());
  dict->SetString("details", event.GetDetails());
  dict->Set("proto", event.GetProtoMessage());
  return dict;
}

}  // namespace syncer
