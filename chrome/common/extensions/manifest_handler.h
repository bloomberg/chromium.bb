// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "base/string16.h"
#include "chrome/common/extensions/manifest.h"

namespace extensions {
class Extension;

// An interface for clients that recognize and parse keys in extension
// manifests.
class ManifestHandler {
 public:
  ManifestHandler();
  virtual ~ManifestHandler();

  // Attempts to parse the extension's manifest.
  // Returns true on success or false on failure; if false, |error| will
  // be set to a failure message.
  virtual bool Parse(Extension* extension, string16* error) = 0;

  // Validate that files associated with this manifest key exist.
  // Validation takes place after parsing. May also append a series of
  // warning messages to |warnings|.
  //
  // Otherwise, returns false, and a description of the error is
  // returned in |error|.
  // TODO(yoz): Change error to string16. See crbug.com/71980.
  virtual bool Validate(const Extension* extension,
                        std::string* error,
                        std::vector<InstallWarning>* warnings) const;

  // If false (the default), only parse the manifest if a registered
  // key is present in the manifest. If true, always attempt to parse
  // the manifest for this extension type, even if no registered keys
  // are present. This allows specifying a default parsed value for
  // extensions that don't declare our key in the manifest.
  // TODO(yoz): Use Feature availability instead.
  virtual bool AlwaysParseForType(Manifest::Type type) const;

  // Same as AlwaysParseForType, but for Validate instead of Parse.
  virtual bool AlwaysValidateForType(Manifest::Type type) const;

  // The list of keys that, if present, should be parsed before calling our
  // Parse (typically, because our Parse needs to read those keys).
  // Defaults to empty.
  virtual const std::vector<std::string> PrerequisiteKeys() const;

  // Associate us with our keys() in the manifest. A handler can register
  // for multiple keys. The global registry takes ownership of this;
  // if it has an existing handler for |key|, it replaces it with this.
  // Typical usage:
  // (new MyManifestHandler)->Register();
  //
  // WARNING: Manifest handlers registered only in the browser process
  // are not available to renderers or utility processes.
  void Register();

  // Call Parse on all registered manifest handlers that should parse
  // this extension.
  static bool ParseExtension(Extension* extension, string16* error);

  // Call Validate on all registered manifest handlers for this extension.
  static bool ValidateExtension(const Extension* extension,
                                std::string* error,
                                std::vector<InstallWarning>* warnings);

 protected:
  // A convenience method for handlers that only register for 1 key,
  // so that they can define keys() { return SingleKey(kKey); }
  static const std::vector<std::string> SingleKey(const std::string& key);

 private:
  // The keys to register us for (in Register).
  virtual const std::vector<std::string> Keys() const = 0;
};

// The global registry for manifest handlers.
class ManifestHandlerRegistry {
 private:
  friend class ManifestHandler;
  friend class ScopedTestingManifestHandlerRegistry;
  friend struct base::DefaultLazyInstanceTraits<ManifestHandlerRegistry>;

  ManifestHandlerRegistry();
  ~ManifestHandlerRegistry();

  void RegisterManifestHandler(const std::string& key,
                               linked_ptr<ManifestHandler> handler);
  bool ParseExtension(Extension* extension, string16* error);
  bool ValidateExtension(const Extension* extension,
                         std::string* error,
                         std::vector<InstallWarning>* warnings);

  // Overrides the current global ManifestHandlerRegistry with
  // |registry|, returning the current one.
  static ManifestHandlerRegistry* SetForTesting(
      ManifestHandlerRegistry* new_registry);

  typedef std::map<std::string, linked_ptr<ManifestHandler> >
      ManifestHandlerMap;
  typedef std::map<ManifestHandler*, int> ManifestHandlerPriorityMap;

  // Puts the manifest handlers in order such that each handler comes after
  // any handlers for their PrerequisiteKeys. If there is no handler for
  // a prerequisite key, that dependency is simply ignored.
  // CHECKs that there are no manifest handlers with circular dependencies.
  void SortManifestHandlers();

  // All registered manifest handlers.
  ManifestHandlerMap handlers_;

  // The priority for each manifest handler. Handlers with lower priority
  // values are evaluated first.
  ManifestHandlerPriorityMap priority_map_;
  bool is_sorted_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLER_H_
