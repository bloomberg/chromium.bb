// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROMEDRIVER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROMEDRIVER_H_

#include <string>

#include "base/memory/scoped_ptr.h"

class CommandExecutor;

// Inits the command executor. Must be called before |ExecuteCommand|.
// This may be called during DLL load on Windows.
void Init(scoped_ptr<CommandExecutor> executor);

// Synchronously executes the given command. Thread safe.
// Command must be a JSON object:
// {
//   "name": <string>,
//   "parameters": <dictionary>,
//   "sessionId": <string>
// }
// Response will always be a JSON object:
// {
//   "status": <integer>,
//   "value": <object>,
//   "sessionId": <string>
// }
// If "status" is non-zero, "value" will be an object with a string "message"
// property which signifies the error message.
void ExecuteCommand(const std::string& command, std::string* response);

// Shuts down the command executor. No commands must be currently executing.
void Shutdown();

#endif  // CHROME_TEST_CHROMEDRIVER_CHROMEDRIVER_H_
