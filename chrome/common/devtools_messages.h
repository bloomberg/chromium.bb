// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_DEVTOOLS_MESSAGES_H_
#define CHROME_COMMON_DEVTOOLS_MESSAGES_H_
#pragma once

#include <map>

#include "ipc/ipc_message_utils.h"

typedef std::map<std::string, std::string> DevToolsRuntimeProperties;

#define MESSAGES_INTERNAL_FILE "chrome/common/devtools_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_DEVTOOLS_MESSAGES_H_
