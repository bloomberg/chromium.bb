// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ports/slot_ref.h"

namespace mojo {
namespace core {
namespace ports {

SlotRef::SlotRef() = default;

SlotRef::SlotRef(const PortRef& port, SlotId slot_id)
    : port_(port), slot_id_(slot_id) {}

SlotRef::~SlotRef() = default;

SlotRef::SlotRef(const SlotRef&) = default;

SlotRef::SlotRef(SlotRef&&) = default;

SlotRef& SlotRef::operator=(const SlotRef&) = default;

SlotRef& SlotRef::operator=(SlotRef&&) = default;

}  // namespace ports
}  // namespace core
}  // namespace mojo
