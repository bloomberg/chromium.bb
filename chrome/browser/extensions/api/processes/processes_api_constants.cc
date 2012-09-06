// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/processes/processes_api_constants.h"

namespace extensions {

namespace processes_api_constants {

// Process object properties.
const char kCpuKey[] = "cpu";
const char kCssCacheKey[] = "cssCache";
const char kFPSKey[] = "fps";
const char kIdKey[] = "id";
const char kImageCacheKey[] = "imageCache";
const char kJsMemoryAllocatedKey[] = "jsMemoryAllocated";
const char kJsMemoryUsedKey[] = "jsMemoryUsed";
const char kNetworkKey[] = "network";
const char kOsProcessIdKey[] = "osProcessId";
const char kPrivateMemoryKey[] = "privateMemory";
const char kProcessesKey[] = "processes";
const char kProfileKey[] = "profile";
const char kScriptCacheKey[] = "scriptCache";
const char kSqliteMemoryKey[] = "sqliteMemory";
const char kTabsListKey[] = "tabs";
const char kTypeKey[] = "type";

// Process types.
const char kProcessTypeBrowser[] = "browser";
const char kProcessTypeExtension[] = "extension";
const char kProcessTypeGPU[] = "gpu";
const char kProcessTypeNacl[] = "nacl";
const char kProcessTypeNotification[] = "notification";
const char kProcessTypeOther[] = "other";
const char kProcessTypePlugin[] = "plugin";
const char kProcessTypeRenderer[] = "renderer";
const char kProcessTypeUtility[] = "utility";
const char kProcessTypeWorker[] = "worker";

// Cache object properties.
const char kCacheLiveSize[] = "liveSize";
const char kCacheSize[] = "size";

// Event names.
const char kOnCreated[] = "experimental.processes.onCreated";
const char kOnExited[] = "experimental.processes.onExited";
const char kOnUnresponsive[] = "experimental.processes.onUnresponsive";
const char kOnUpdated[] = "experimental.processes.onUpdated";
const char kOnUpdatedWithMemory[] =
    "experimental.processes.onUpdatedWithMemory";

// Error strings.
const char kExtensionNotSupported[] =
    "The Processes extension API is not supported on this platform.";
const char kProcessNotFound[] = "Process not found: *.";

}  // namespace processes_api_constants

}  // namespace extensions
