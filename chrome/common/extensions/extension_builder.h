// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_BUILDER_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_BUILDER_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/value_builder.h"

namespace extensions {
class Extension;

// An easier way to create extensions than Extension::Create.  The
// constructor sets up some defaults which are customized using the
// methods.  The only method that must be called is SetManifest().
class ExtensionBuilder {
 public:
  ExtensionBuilder();
  ~ExtensionBuilder();

  // Can only be called once, after which it's invalid to use the builder.
  // CHECKs that the extension was created successfully.
  scoped_refptr<Extension> Build();

  // Defaults to FilePath().
  ExtensionBuilder& SetPath(const base::FilePath& path);

  // Defaults to Manifest::LOAD.
  ExtensionBuilder& SetLocation(Manifest::Location location);

  ExtensionBuilder& SetManifest(scoped_ptr<base::DictionaryValue> manifest);
  ExtensionBuilder& SetManifest(DictionaryBuilder& manifest_builder) {
    return SetManifest(manifest_builder.Build());
  }

  ExtensionBuilder& AddFlags(int init_from_value_flags);

  // Defaults to the default extension ID created in Extension::Create.
  ExtensionBuilder& SetID(const std::string& id);

 private:
  base::FilePath path_;
  Manifest::Location location_;
  scoped_ptr<base::DictionaryValue> manifest_;
  int flags_;
  std::string id_;
};

} // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_BUILDER_H_
