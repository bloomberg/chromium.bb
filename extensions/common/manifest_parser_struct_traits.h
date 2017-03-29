// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_PARSER_STRUCT_TRAITS_H
#define EXTENSIONS_COMMON_MANIFEST_PARSER_STRUCT_TRAITS_H

#include "extensions/common/manifest_parser.mojom.h"
#include "extensions/common/update_manifest.h"

namespace mojo {

template <>
struct StructTraits<::extensions::mojom::UpdateManifestResults::DataView,
                    ::UpdateManifest::Results> {
  static const std::vector<::UpdateManifest::Result>& list(
      const ::UpdateManifest::Results& input) {
    return input.list;
  }

  static int daystart_elapsed_seconds(const ::UpdateManifest::Results& input) {
    return input.daystart_elapsed_seconds;
  }

  static bool Read(::extensions::mojom::UpdateManifestResults::DataView input,
                   ::UpdateManifest::Results* output);
};

template <>
struct StructTraits<::extensions::mojom::UpdateManifestResult::DataView,
                    ::UpdateManifest::Result> {
  static const std::string& extension_id(
      const ::UpdateManifest::Result& input) {
    return input.extension_id;
  }

  static const std::string& version(const ::UpdateManifest::Result& input) {
    return input.version;
  }

  static const std::string& browser_min_version(
      const ::UpdateManifest::Result& input) {
    return input.browser_min_version;
  }

  static const GURL& crx_url(const ::UpdateManifest::Result& input) {
    return input.crx_url;
  }

  static const std::string& package_hash(
      const ::UpdateManifest::Result& input) {
    return input.package_hash;
  }

  static int size(const ::UpdateManifest::Result& input) { return input.size; }

  static const std::string& package_fingerprint(
      const ::UpdateManifest::Result& input) {
    return input.package_fingerprint;
  }

  static const GURL& diff_crx_url(const ::UpdateManifest::Result& input) {
    return input.diff_crx_url;
  }

  static const std::string& diff_package_hash(
      const ::UpdateManifest::Result& input) {
    return input.diff_package_hash;
  }

  static int diff_size(const ::UpdateManifest::Result& input) {
    return input.diff_size;
  }

  static bool Read(::extensions::mojom::UpdateManifestResult::DataView input,
                   ::UpdateManifest::Result* output);
};

}  // namespace mojo

#endif  // EXTENSIONS_COMMON_MANIFEST_PARSER_STRUCT_TRAITS_H
