// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_NAMESPACE_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_NAMESPACE_H_
#pragma once

#include <string>

namespace extensions {

namespace settings_namespace {

// The namespaces of the storage areas.
enum Namespace {
  LOCAL,  // "local" i.e. chrome.experimental.storage.local
  SYNC,   // "sync"  i.e. chrome.experimental.storage.sync
  INVALID
};

// Converts a namespace to its string representation.
// Namespace must not be INVALID.
std::string ToString(Namespace settings_namespace);

// Converts a string representation of a namespace to its namespace, or INVALID
// if the string doesn't map to one.
Namespace FromString(const std::string& ns_string);

}  // namespace settings_namespace

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_NAMESPACE_H_
