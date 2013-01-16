// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLER_HELPERS_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLER_HELPERS_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"

namespace base {
class DictionaryValue;
}

class ExtensionIconSet;

namespace extensions {

struct ActionInfo;
class Extension;

namespace manifest_handler_helpers {

// Strips leading slashes from the file path. Returns true iff the final path is
// non empty.
bool NormalizeAndValidatePath(std::string* path);

// Loads icon paths defined in dictionary |icons_value| into ExtensionIconSet
// |icons|. |icons_value| is a dictionary value {icon size -> icon path}. Icons
// in |icons_value| whose size is not in |icon_sizes| will be ignored.
// Returns success. If load fails, |error| will be set.
bool LoadIconsFromDictionary(const base::DictionaryValue* icons_value,
                             const int* icon_sizes,
                             size_t num_icon_sizes,
                             ExtensionIconSet* icons,
                             string16* error);

// TODO(rdevlin.cronin): Move this to a helper file in
// chrome/common/extensions/api/extension_action/ when the Extension class stops
// using it.
scoped_ptr<ActionInfo> LoadActionInfo(
    const Extension* extension,
    const base::DictionaryValue* manifest_section,
    string16* error);

}  // namespace manifest_handler_helpers
}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLER_HELPERS_H_
