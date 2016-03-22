// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PLUGIN_CONSTANTS_WIN_H_
#define CONTENT_COMMON_PLUGIN_CONSTANTS_WIN_H_

#include "base/strings/string16.h"
#include "build/build_config.h"

#if !defined(OS_WIN)
#error "Windows-only header"
#endif

namespace content {

// The name of the registry key which NPAPI plugins update on installation.
extern const base::char16 kRegistryMozillaPlugins[];

extern const base::char16 kMozillaActiveXPlugin[];
extern const base::char16 kNewWMPPlugin[];
extern const base::char16 kOldWMPPlugin[];
extern const base::char16 kYahooApplicationStatePlugin[];
extern const base::char16 kWanWangProtocolHandlerPlugin[];
extern const base::char16 kJavaPlugin1[];
extern const base::char16 kJavaPlugin2[];

}  // namespace content

#endif  // CONTENT_COMMON_PLUGIN_CONSTANTS_WIN_H_
