// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_SESSION_CONFIGURATOR_COMMON_NETWORK_SWITCHES_H_
#define COMPONENTS_NETWORK_SESSION_CONFIGURATOR_COMMON_NETWORK_SWITCHES_H_

namespace base {
class CommandLine;
}

namespace switches {

#define NETWORK_SWITCH(name, value) extern const char name[];
#include "components/network_session_configurator/common/network_switch_list.h"
#undef NETWORK_SWITCH

}  // namespace switches

namespace network_session_configurator {

// Copies all command line switches the configurator handles from the |src|
// CommandLine to the |dest| one.
void CopyNetworkSwitches(const base::CommandLine& src_command_line,
                         base::CommandLine* dest_command_line);

}  // namespace network_session_configurator

#endif  // COMPONENTS_NETWORK_SESSION_CONFIGURATOR_COMMON_NETWORK_SWITCHES_H_
