// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/encoded_view.h"

#include <algorithm>

#include "base/logging.h"

namespace zucchini {

EncodedView::EncodedView(const ImageIndex* image_index)
    : image_index_(image_index) {}

// static
EncodedView::value_type EncodedView::Projection(const ImageIndex& image_index,
                                                offset_t location) {
  DCHECK_LT(location, image_index.size());

  // Find out what lies at |location|.
  TypeTag type = image_index.GetType(location);

  // |location| points into raw data.
  if (type == kNoTypeTag) {
    // The projection is the identity function on raw content.
    return image_index.GetRawValue(location);
  }

  // |location| points into a Reference.
  Reference ref = image_index.FindReference(type, location);
  DCHECK_GE(location, ref.location);
  DCHECK_LT(location, ref.location + image_index.GetTraits(type).width);

  // |location| is not the first byte of the reference.
  if (location != ref.location) {
    // Trailing bytes of a reference are all projected to the same value.
    return kReferencePaddingProjection;
  }

  // Targets with an associated Label will use its Label index in projection.
  // Otherwise, LabelBound() is used for all targets with no Label.
  size_t target = IsMarked(ref.target)
                      ? UnmarkIndex(ref.target)
                      : image_index.LabelBound(image_index.GetPoolTag(type));

  // Projection is done on (|target|, |type|), shifted by a constant value to
  // avoid collisions with raw content.
  size_t projection = target;
  projection *= image_index.TypeCount();
  projection += type.value();
  return projection + kBaseReferenceProjection;
}

size_t EncodedView::Cardinality() const {
  size_t max_width = 0;
  for (uint8_t pool = 0; pool < image_index_->PoolCount(); ++pool) {
    // LabelBound() + 1 for the extra case of references with no Label.
    max_width =
        std::max(image_index_->LabelBound(PoolTag(pool)) + 1, max_width);
  }
  return max_width * image_index_->TypeCount() + kBaseReferenceProjection;
}

}  // namespace zucchini
