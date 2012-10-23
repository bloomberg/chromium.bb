// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROMEDRIVER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROMEDRIVER_H_

#include <string>

class CommandExecutor;

typedef CommandExecutor* (*CommandExecutorFactoryFunc)();

// Sets the function to use for creating new |CommandExecutor|s. Only
// for testing purposes.
void SetCommandExecutorFactoryForTesting(CommandExecutorFactoryFunc func);

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

#endif  // CHROME_TEST_CHROMEDRIVER_CHROMEDRIVER_H_
