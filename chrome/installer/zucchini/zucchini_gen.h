// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_GEN_H_
#define CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_GEN_H_

#include <vector>

#include "chrome/installer/zucchini/image_utils.h"
#include "chrome/installer/zucchini/zucchini.h"

namespace zucchini {

class EquivalenceMap;
class ImageIndex;
class PatchElementWriter;

// Writes equivalences from |equivalence_map|, and extra data from |new_image|
// found in gaps between equivalences to |patch_writer|.
bool GenerateEquivalencesAndExtraData(ConstBufferView new_image,
                                      const EquivalenceMap& equivalence_map,
                                      PatchElementWriter* patch_writer);

// Writes raw delta between |old_image| and |new_image| matched by
// |equivalence_map| to |patch_writer|, using |new_index| to ignore reference
// bytes.
bool GenerateRawDelta(ConstBufferView old_image,
                      ConstBufferView new_image,
                      const EquivalenceMap& equivalence_map,
                      const ImageIndex& new_index,
                      PatchElementWriter* patch_writer);

// Generates raw patch element data between |old_image| and |new_image|, and
// writes them to |patch_writer|. |old_sa| is the suffix array for |old_image|.
bool GenerateRawElement(std::vector<offset_t> old_sa,
                        ConstBufferView old_image,
                        ConstBufferView new_image,
                        PatchElementWriter* patch_writer);

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_GEN_H_
