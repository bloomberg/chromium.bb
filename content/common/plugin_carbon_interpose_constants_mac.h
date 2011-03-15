// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PLUGIN_CARBON_INTERPOSE_CONSTANTS_MAC_H_
#define CONTENT_COMMON_PLUGIN_CARBON_INTERPOSE_CONSTANTS_MAC_H_
#pragma once

#if !defined(__LP64__)

// Strings used in setting up Carbon interposing for the plugin process.
namespace plugin_interpose_strings {

extern const char kDYLDInsertLibrariesKey[];
extern const char kInterposeLibraryPath[];

}  // namespace plugin_interpose_strings

#endif  // !__LP64__

#endif  // CONTENT_COMMON_PLUGIN_CARBON_INTERPOSE_CONSTANTS_MAC_H_
