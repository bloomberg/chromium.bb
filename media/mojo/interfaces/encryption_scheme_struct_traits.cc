// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/interfaces/encryption_scheme_struct_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::PatternDataView,
                  media::EncryptionScheme::Pattern>::
    Read(media::mojom::PatternDataView input,
         media::EncryptionScheme::Pattern* output) {
  *output = media::EncryptionScheme::Pattern(input.encrypt_blocks(),
                                             input.skip_blocks());
  return true;
}

// static
bool StructTraits<
    media::mojom::EncryptionSchemeDataView,
    media::EncryptionScheme>::Read(media::mojom::EncryptionSchemeDataView input,
                                   media::EncryptionScheme* output) {
  media::EncryptionScheme::CipherMode mode;
  if (!input.ReadMode(&mode))
    return false;

  media::EncryptionScheme::Pattern pattern;
  if (!input.ReadPattern(&pattern))
    return false;

  *output = media::EncryptionScheme(mode, pattern);

  return true;
}

}  // namespace mojo