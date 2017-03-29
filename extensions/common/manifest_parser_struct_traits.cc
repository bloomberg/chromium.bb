// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_parser_struct_traits.h"
#include "url/mojo/url_gurl_struct_traits.h"

namespace mojo {

// static
bool StructTraits<::extensions::mojom::UpdateManifestResults::DataView,
                  ::UpdateManifest::Results>::
    Read(::extensions::mojom::UpdateManifestResults::DataView input,
         ::UpdateManifest::Results* output) {
  if (!input.ReadList(&output->list))
    return false;
  output->daystart_elapsed_seconds = input.daystart_elapsed_seconds();
  return true;
}

// static
bool StructTraits<::extensions::mojom::UpdateManifestResult::DataView,
                  ::UpdateManifest::Result>::
    Read(::extensions::mojom::UpdateManifestResult::DataView input,
         ::UpdateManifest::Result* output) {
  if (!input.ReadExtensionId(&output->extension_id))
    return false;
  if (!input.ReadVersion(&output->version))
    return false;
  if (!input.ReadBrowserMinVersion(&output->browser_min_version))
    return false;
  if (!input.ReadCrxUrl(&output->crx_url))
    return false;
  if (!input.ReadPackageHash(&output->package_hash))
    return false;
  output->size = input.size();
  if (!input.ReadPackageFingerprint(&output->package_fingerprint))
    return false;
  if (!input.ReadDiffCrxUrl(&output->diff_crx_url))
    return false;
  if (!input.ReadDiffPackageHash(&output->diff_package_hash))
    return false;
  output->diff_size = input.diff_size();
  return true;
}

}  // namespace mojo
