// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MACH_PORT_MAC_H_
#define IPC_MACH_PORT_MAC_H_

#include <mach/mach.h>

#include "ipc/ipc_export.h"
#include "ipc/ipc_message_macros.h"

namespace IPC {

// MachPortMac is a wrapper around an OSX mach_port_t that can be transported
// across Chrome IPC channels that support attachment brokering. The mach_port_t
// will be duplicated into the destination process by the attachment broker.
class IPC_EXPORT MachPortMac {
 public:
  MachPortMac() : mach_port_(MACH_PORT_NULL) {}
  explicit MachPortMac(const mach_port_t& mach_port) : mach_port_(mach_port) {}

  mach_port_t get_mach_port() const { return mach_port_; }
  void set_mach_port(mach_port_t mach_port) { mach_port_ = mach_port; }

 private:
  mach_port_t mach_port_;
};

template <>
struct IPC_EXPORT ParamTraits<MachPortMac> {
  typedef MachPortMac param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // IPC_MACH_PORT_MAC_H_
