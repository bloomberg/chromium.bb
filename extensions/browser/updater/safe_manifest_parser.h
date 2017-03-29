// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_SAFE_MANIFEST_PARSER_H_
#define EXTENSIONS_BROWSER_UPDATER_SAFE_MANIFEST_PARSER_H_

#include <string>

#include "base/callback_forward.h"
#include "extensions/common/update_manifest.h"

namespace extensions {

// Parses an update manifest |xml| safely in a utility process and calls
// |callback| with the results, which will be null on failure. Runs on
// the UI thread.
void ParseUpdateManifest(
    const std::string& xml,
    const base::Callback<void(const UpdateManifest::Results*)>& callback);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_SAFE_MANIFEST_PARSER_H_
