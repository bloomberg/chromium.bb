// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_PROCESS_POLICY_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_PROCESS_POLICY_H_
#pragma once

class ExtensionSet;
class ExtensionURLInfo;

namespace extensions {

class Extension;

// Returns the extension for the given URL.  Excludes extension objects for
// bookmark apps, which do not use the app process model.
const Extension* GetNonBookmarkAppExtension(const ExtensionSet& extensions,
                                            const ExtensionURLInfo& url);

// Check if navigating a toplevel page from |old_url| to |new_url| would cross
// an extension process boundary (e.g. navigating from a web URL into an
// extension URL).
bool CrossesExtensionProcessBoundary(
    const ExtensionSet& extensions,
    const ExtensionURLInfo& old_url,
    const ExtensionURLInfo& new_url);

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_PROCESS_POLICY_H_
