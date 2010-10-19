// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Processes API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESSES_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESSES_API_CONSTANTS_H_
#pragma once

namespace extension_processes_api_constants {

// Keys used in serializing process data & events.
extern const char kCpuKey[];
extern const char kIdKey[];
extern const char kNetworkKey[];
extern const char kPrivateMemoryKey[];
extern const char kProcessesKey[];
extern const char kSharedMemoryKey[];
extern const char kTypeKey[];

extern const char kProcessTypeBrowser[];
extern const char kProcessTypeExtension[];
extern const char kProcessTypeGPU[];
extern const char kProcessTypeNacl[];
extern const char kProcessTypeNotification[];
extern const char kProcessTypeOther[];
extern const char kProcessTypePlugin[];
extern const char kProcessTypeRenderer[];
extern const char kProcessTypeUtility[];
extern const char kProcessTypeWorker[];

extern const char kOnUpdated[];

};  // namespace extension_processes_api_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESSES_API_CONSTANTS_H_
