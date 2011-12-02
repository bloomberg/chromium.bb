// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_H_
#pragma once

#include <map>
#include <string>
#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace extensions {

// Lightweight wrapper around a DictionaryValue representing an extension's
// manifest. Currently enforces access to properties of the manifest based
// on manifest type.
//
// TODO(aa): Move more smarts about mmanifest into this class over time.
class Manifest {
 public:
  // Flags for matching types of extension manifests.
  enum Type {
    kTypeNone = 0,

    // Extension::TYPE_EXTENSION and Extension::TYPE_USER_SCRIPT
    kTypeExtension = 1 << 0,

    // Extension::TYPE_THEME
    kTypeTheme = 1 << 1,

    // Extension::TYPE_HOSTED_APP
    kTypeHostedApp = 1 << 2,

    // Extension::TYPE_PACKAGED_APP
    kTypePackagedApp = 1 << 3,

    // Extension::TYPE_PLATFORM_APP
    kTypePlatformApp = 1 << 4,

    // All types
    kTypeAll = (1 << 5) - 1,
  };

  // Returns all known keys (this is used for testing).
  static std::set<std::string> GetAllKnownKeys();

  // Takes over ownership of |value|.
  explicit Manifest(base::DictionaryValue* value);
  virtual ~Manifest();

  // Returns true if all keys in the manifest can be specified by
  // the extension type.
  bool ValidateManifest(std::string* error) const;

  // Returns the manifest type.
  Type GetType() const;

  // Returns true if the manifest represents an Extension::TYPE_THEME.
  bool IsTheme() const;

  // Returns true for Extension::TYPE_PLATFORM_APP
  bool IsPlatformApp() const;

  // Returns true for Extension::TYPE_PACKAGED_APP.
  bool IsPackagedApp() const;

  // Returns true for Extension::TYPE_HOSTED_APP.
  bool IsHostedApp() const;

  // These access the wrapped manifest value, returning false when the property
  // does not exist or if the manifest type can't access it.
  bool HasKey(const std::string& key) const;
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
  // Note: only know this when you KNOW you don't need the validation.
  base::DictionaryValue* value() const { return value_.get(); }

 private:
  // Returns true if the extension can specify the given |path|.
  bool CanAccessPath(const std::string& path) const;

  scoped_ptr<base::DictionaryValue> value_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_H_
