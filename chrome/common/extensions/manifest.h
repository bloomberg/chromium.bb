// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_H_

#include <map>
#include <string>
#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"

namespace extensions {

// Wraps the DictionaryValue form of extension's manifest. Enforces access to
// properties of the manifest using ManifestFeatureProvider.
class Manifest {
 public:
  Manifest(Extension::Location location, scoped_ptr<DictionaryValue> value);
  virtual ~Manifest();

  const std::string& extension_id() const { return extension_id_; }
  void set_extension_id(const std::string& id) { extension_id_ = id; }

  Extension::Location location() const { return location_; }

  // |error| will be non-empty if the manifest is malformed. |warnings| will
  // be populated if there are keys in the manifest that cannot be specified by
  // the extension type.
  void ValidateManifest(std::string* error,
                        Extension::InstallWarningVector* warnings) const;

  // The version of this extension's manifest. We increase the manifest
  // version when making breaking changes to the extension system. If the
  // manifest contains no explicit manifest version, this returns the current
  // system default.
  int GetManifestVersion() const;

  // Returns the manifest type.
  Extension::Type type() const { return type_; }

  bool is_theme() const { return type_ == Extension::TYPE_THEME; }
  bool is_platform_app() const { return type_ == Extension::TYPE_PLATFORM_APP; }
  bool is_hosted_app() const { return type_ == Extension::TYPE_HOSTED_APP; }
  bool is_legacy_packaged_app() const {
    return type_ == Extension::TYPE_LEGACY_PACKAGED_APP;
  }

  // These access the wrapped manifest value, returning false when the property
  // does not exist or if the manifest type can't access it.
  bool HasKey(const std::string& key) const;
  bool HasPath(const std::string& path) const;
  bool Get(const std::string& path, base::Value** out_value) const;
  bool GetBoolean(const std::string& path, bool* out_value) const;
  bool GetInteger(const std::string& path, int* out_value) const;
  bool GetString(const std::string& path, std::string* out_value) const;
  bool GetString(const std::string& path, string16* out_value) const;
  bool GetDictionary(const std::string& path,
                     base::DictionaryValue** out_value) const;
  bool GetList(const std::string& path, base::ListValue** out_value) const;

  // Returns a new Manifest equal to this one, passing ownership to
  // the caller.
  Manifest* DeepCopy() const;

  // Returns true if this equals the |other| manifest.
  bool Equals(const Manifest* other) const;

  // Gets the underlying DictionaryValue representing the manifest.
  // Note: only use this when you KNOW you don't need the validation.
  const base::DictionaryValue* value() const { return value_.get(); }

 private:
  // Returns true if the extension can specify the given |path|.
  bool CanAccessPath(const std::string& path) const;
  bool CanAccessKey(const std::string& key) const;

  // A persistent, globally unique ID. An extension's ID is used in things
  // like directory structures and URLs, and is expected to not change across
  // versions. It is generated as a SHA-256 hash of the extension's public
  // key, or as a hash of the path in the case of unpacked extensions.
  std::string extension_id_;

  // The location the extension was loaded from.
  Extension::Location location_;

  // The underlying dictionary representation of the manifest.
  scoped_ptr<base::DictionaryValue> value_;

  Extension::Type type_;

  DISALLOW_COPY_AND_ASSIGN(Manifest);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_H_
