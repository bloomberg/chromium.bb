// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_UTILITY_UTILITY_HANDLER_H_
#define EXTENSIONS_UTILITY_UTILITY_HANDLER_H_

#include <string>

#include "base/macros.h"

namespace IPC {
class Message;
}

namespace service_manager {
class InterfaceRegistry;
}

namespace extensions {

// A handler for extensions-related IPC from within utility processes.
class UtilityHandler {
 public:
  UtilityHandler();
  ~UtilityHandler();

  static void UtilityThreadStarted();

  static void ExposeInterfacesToBrowser(
      service_manager::InterfaceRegistry* registry,
      bool running_elevated);

  bool OnMessageReceived(const IPC::Message& message);

 private:
  // IPC message handlers.
  void OnParseUpdateManifest(const std::string& xml);

  DISALLOW_COPY_AND_ASSIGN(UtilityHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_UTILITY_UTILITY_HANDLER_H_
