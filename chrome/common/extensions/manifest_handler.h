// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLER_H_

#include <string>
#include <vector>

#include "base/string16.h"

namespace base {
class Value;
}

namespace extensions {

class Extension;

class ManifestHandler {
 public:
  ManifestHandler();
  virtual ~ManifestHandler();

  // Attempts to parse the manifest value.
  // Returns true on success or false on failure; if false, |error| will
  // be set to a failure message.
  virtual bool Parse(const base::Value* value,
                     Extension* extension,
                     string16* error) = 0;

  // Perform any initialization which is necessary when the Handler's key is
  // not present in the manifest.
  // Returns true on success or false on failure; if false, |error| will
  // be set to a failure message.
  virtual bool HasNoKey(Extension* extension, string16* error);

  // Associate |handler| with |key| in the manifest. Takes ownership
  // of |handler|. TODO(yoz): Decide how to handle dotted subkeys.
  // WARNING: Manifest handlers registered only in the browser process
  // are not available to renderers.
  static void Register(const std::string& key, ManifestHandler* handler);

  // Get the manifest handler associated with |key|, or NULL
  // if there is none.
  static ManifestHandler* Get(const std::string& key);

  // If the handler is not handling most of the keys, it may be
  // more efficient to have a list of keys to iterate over.
  // TODO(yoz): this isn't the long-term solution.
  static std::vector<std::string> GetKeys();
};


}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLER_H_
