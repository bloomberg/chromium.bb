// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/common/shell_app_runtime.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension.h"

namespace extensions {

// static
const char* ShellAppRuntime::GetName() { return "app.runtime"; }

// static
const char* ShellAppRuntime::GetSchema() {
  return "["
         "  {"
         "    \"compiler_options\": {},"
         "    \"functions\": [],"
         "    \"deprecated\": null,"
         "    \"nodoc\": false,"
         "    \"platforms\": null,"
         "    \"internal\": false,"
         "    \"namespace\": \"app.runtime\","
         "    \"events\": ["
         "      {"
         "        \"type\": \"function\","
         "        \"name\": \"onLaunched\","
         "        \"parameters\": ["
         "          {"
         "            \"optional\": true,"
         "            \"name\": \"launchDataPlaceholder\""  // No launch data.
         "          }"
         "        ]"
         "      }"
         "    ]"
         "  }"
         "]";
}

}  // namespace extensions
