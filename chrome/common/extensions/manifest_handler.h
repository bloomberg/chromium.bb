// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLER_H_

#include <set>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/string16.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest.h"

namespace extensions {

class ManifestHandler {
 public:
  ManifestHandler();
  virtual ~ManifestHandler();

  // Attempts to parse the extension's manifest.
  // Returns true on success or false on failure; if false, |error| will
  // be set to a failure message.
  virtual bool Parse(Extension* extension, string16* error) = 0;

  // If false (the default), only parse the manifest if a registered
  // key is present in the manifest. If true, always attempt to parse
  // the manifest for this extension type, even if no registered keys
  // are present. This allows specifying a default parsed value for
  // extensions that don't declare our key in the manifest.
  // TODO(yoz): Use Feature availability instead.
  virtual bool AlwaysParseForType(Manifest::Type type);

  // The list of keys that, if present, should be parsed before calling our
  // Parse (typically, because our Parse needs to read those keys).
  // Defaults to empty.
  virtual const std::vector<std::string>& PrerequisiteKeys();

  // Associate |handler| with |key| in the manifest. A handler can register
  // for multiple keys. The global registry takes ownership of |handler|;
  // if it has an existing handler for |key|, it replaces it with this one.
  //
  // WARNING: Manifest handlers registered only in the browser process
  // are not available to renderers or utility processes.
  static void Register(const std::string& key,
                       linked_ptr<ManifestHandler> handler);

  // Call Parse on all registered manifest handlers that should parse
  // this extension.
  static bool ParseExtension(Extension* extension, string16* error);

  // Reset the manifest handler registry to an empty state. Useful for
  // unit tests.
  static void ClearRegistryForTesting();
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLER_H_
