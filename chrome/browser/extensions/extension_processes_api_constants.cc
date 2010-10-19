// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_processes_api_constants.h"

namespace extension_processes_api_constants {

const char kCpuKey[] = "cpu";
const char kIdKey[] = "id";
const char kNetworkKey[] = "network";
const char kPrivateMemoryKey[] = "privateMemory";
const char kProcessesKey[] = "processes";
const char kSharedMemoryKey[] = "sharedMemory";
const char kTypeKey[] = "type";

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

const char kOnUpdated[] = "experimental.processes.onUpdated";

}  // namespace extension_processes_api_constants
