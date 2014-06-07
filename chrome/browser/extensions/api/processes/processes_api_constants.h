// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Processes API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROCESSES_PROCESSES_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_API_PROCESSES_PROCESSES_API_CONSTANTS_H_

namespace extensions {

namespace processes_api_constants {

// Process object properties.
extern const char kCpuKey[];
extern const char kCssCacheKey[];
extern const char kIdKey[];
extern const char kImageCacheKey[];
extern const char kJsMemoryAllocatedKey[];
extern const char kJsMemoryUsedKey[];
extern const char kNaClDebugPortKey[];
extern const char kNetworkKey[];
extern const char kOsProcessIdKey[];
extern const char kPrivateMemoryKey[];
extern const char kProfileKey[];
extern const char kScriptCacheKey[];
extern const char kSqliteMemoryKey[];
extern const char kTabsListKey[];
extern const char kTitleKey[];
extern const char kTypeKey[];

// Process types.
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

// Cache object properties.
extern const char kCacheLiveSize[];
extern const char kCacheSize[];

// Event names.
extern const char kOnCreated[];
extern const char kOnExited[];
extern const char kOnUnresponsive[];
extern const char kOnUpdated[];
extern const char kOnUpdatedWithMemory[];

// Error strings.
extern const char kExtensionNotSupported[];
extern const char kProcessNotFound[];

}  // namespace processes_api_constants

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROCESSES_PROCESSES_API_CONSTANTS_H_
