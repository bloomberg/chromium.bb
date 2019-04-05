// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_SLOT_REF_H_
#define MOJO_CORE_PORTS_SLOT_REF_H_

#include "base/component_export.h"
#include "mojo/core/ports/event.h"
#include "mojo/core/ports/port_ref.h"

namespace mojo {
namespace core {
namespace ports {

// A reference to a specific slot on a specific port.
class COMPONENT_EXPORT(MOJO_CORE_PORTS) SlotRef {
 public:
  SlotRef();
  SlotRef(const PortRef& port, SlotId slot_id);
  ~SlotRef();

  SlotRef(const SlotRef& other);
  SlotRef(SlotRef&& other);

  SlotRef& operator=(const SlotRef& other);
  SlotRef& operator=(SlotRef&& other);

  const PortRef& port() const { return port_; }
  SlotId slot_id() const { return slot_id_; }

  bool is_valid() const { return port_.is_valid(); }

 private:
  PortRef port_;
  SlotId slot_id_;
};

}  // namespace ports
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PORTS_SLOT_REF_H_
