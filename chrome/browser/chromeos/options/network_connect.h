// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONNECT_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONNECT_H_

#include <string>

#include "ui/gfx/native_widget_types.h"  // gfx::NativeWindow

namespace chromeos {
namespace network_connect {

enum ConnectResult {
  NETWORK_NOT_FOUND,
  CONNECT_NOT_STARTED,
  CONNECT_STARTED
};

// Attempts to connect to the network specified by |service_path|.
// Returns one of the following results:
//  NETWORK_NOT_FOUND if the network does not exist.
//  CONNECT_NOT_STARTED if no connection attempt was started, e.g. because the
//   network is already connected, connecting, or activating.
//  CONNECT_STARTED if a connection attempt was started.
ConnectResult ConnectToNetwork(const std::string& service_path,
                               gfx::NativeWindow parent_window);

}  // namespace network_connect
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONNECT_H_
