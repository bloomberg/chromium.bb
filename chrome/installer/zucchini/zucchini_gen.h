// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_GEN_H_
#define CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_GEN_H_

#include <vector>

#include "base/optional.h"
#include "chrome/installer/zucchini/buffer_view.h"
#include "chrome/installer/zucchini/image_utils.h"
#include "chrome/installer/zucchini/zucchini.h"

namespace zucchini {

class EquivalenceMap;
class ImageIndex;
class PatchElementWriter;
class ReferenceDeltaSink;
class ReferenceSet;
class UnorderedLabelManager;

// Projects targets in |old_targets| to a list of new targets using
// |equivalences|. Targets that cannot be projected have offset assigned as
// |kUnusedIndex|. Returns the list of new targets in a new vector.
// |old_targets| must be sorted in ascending order and |equivalence| must be
// sorted in ascending order of |Equivalence::src_offset|.
std::vector<offset_t> MakeNewTargetsFromEquivalenceMap(
    const std::vector<offset_t>& old_targets,
    const std::vector<Equivalence>& equivalences);

// Extract references in |new_references| that have the following properties:
// - The location is found in |equivalences| (dst).
// - The target (key) is absent in |new_label_manager|.
// The targets of the extracted references are returned in a new vector.
std::vector<offset_t> FindExtraTargets(
    const ReferenceSet& new_references,
    const UnorderedLabelManager& new_label_manager,
    const EquivalenceMap& equivalence_map);

// Creates an EquivalenceMap from "old" image to "new" image and returns the
// result. The params |*_image_index|:
// - Provide "old" and "new" raw image data and references.
// - Mediate Label matching, which links references between "old" and "new", and
//   guides EquivalenceMap construction.
EquivalenceMap CreateEquivalenceMap(const ImageIndex& old_image_index,
                                    const ImageIndex& new_image_index);

// Writes equivalences from |equivalence_map|, and extra data from |new_image|
// found in gaps between equivalences to |patch_writer|.
bool GenerateEquivalencesAndExtraData(ConstBufferView new_image,
                                      const EquivalenceMap& equivalence_map,
                                      PatchElementWriter* patch_writer);

// Writes raw delta between |old_image| and |new_image| matched by
// |equivalence_map| to |patch_writer|, using |new_image_index| to ignore
// reference bytes.
bool GenerateRawDelta(ConstBufferView old_image,
                      ConstBufferView new_image,
                      const EquivalenceMap& equivalence_map,
                      const ImageIndex& new_image_index,
                      PatchElementWriter* patch_writer);

// Writes reference delta between references from |old_refs| and from
// |new_refs| to |patch_writer|. |new_label_manager| contains projected
// labels from old to new image for references pool associated with |new_refs|
bool GenerateReferencesDelta(const ReferenceSet& old_refs,
                             const ReferenceSet& new_refs,
                             const UnorderedLabelManager& new_label_manager,
                             const EquivalenceMap& equivalence_map,
                             ReferenceDeltaSink* reference_delta_sink);

// Writes |extra_targets| associated with |pool_tag| to |patch_writer|.
bool GenerateExtraTargets(const std::vector<offset_t>& extra_targets,
                          PoolTag pool_tag,
                          PatchElementWriter* patch_writer);

// Generates raw patch element data between |old_image| and |new_image|, and
// writes them to |patch_writer|. |old_sa| is the suffix array for |old_image|.
bool GenerateRawElement(const std::vector<offset_t>& old_sa,
                        ConstBufferView old_image,
                        ConstBufferView new_image,
                        PatchElementWriter* patch_writer);

// Generates patch element of type |exe_type| from |old_image| to |new_image|,
// and writes it to |patch_writer|.
bool GenerateExecutableElement(ExecutableType exe_type,
                               ConstBufferView old_image,
                               ConstBufferView new_image,
                               PatchElementWriter* patch_writer);

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_GEN_H_
