// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_URLS_H_
#define EXTENSIONS_COMMON_EXTENSION_URLS_H_

#include "base/strings/string16.h"

namespace extensions {

// The name of the event_bindings module.
extern const char kEventBindings[];

// The name of the schemaUtils module.
extern const char kSchemaUtils[];

// Determine whether or not a source came from an extension. |source| can link
// to a page or a script, and can be external (e.g., "http://www.google.com"),
// extension-related (e.g., "chrome-extension://<extension_id>/background.js"),
// or internal (e.g., "event_bindings" or "schemaUtils").
bool IsSourceFromAnExtension(const base::string16& source);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSION_URLS_H_
