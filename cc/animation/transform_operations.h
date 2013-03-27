// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_TRANSFORM_OPERATIONS_H_
#define CC_ANIMATION_TRANSFORM_OPERATIONS_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "cc/animation/transform_operation.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/transform.h"

namespace gfx {
struct DecomposedTransform;
}

namespace cc {

// Transform operations are a decomposed transformation matrix. It can be
// applied to obtain a gfx::Transform at any time, and can be blended
// intelligently with other transform operations, so long as they represent the
// same decomposition. For example, if we have a transform that is made up of
// a rotation followed by skew, it can be blended intelligently with another
// transform made up of a rotation followed by a skew. Blending is possible if
// we have two dissimilar sets of transform operations, but the effect may not
// be what was intended. For more information, see the comments for the blend
// function below.
class CC_EXPORT TransformOperations {
 public:
  TransformOperations();
  TransformOperations(const TransformOperations& other);
  ~TransformOperations();

  // Returns a transformation matrix representing these transform operations.
  gfx::Transform Apply() const;

  // Given another set of transform operations and a progress in the range
  // [0, 1], returns a transformation matrix representing the intermediate
  // value. If this->MatchesTypes(from), then each of the operations are
  // blended separately and then combined. Otherwise, the two sets of
  // transforms are baked to matrices (using apply), and the matrices are
  // then decomposed and interpolated. For more information, see
  // http://www.w3.org/TR/2011/WD-css3-2d-transforms-20111215/#matrix-decomposition.
  gfx::Transform Blend(const TransformOperations& from, double progress) const;

  // Returns true if this operation and its descendants have the same types
  // as other and its descendants.
  bool MatchesTypes(const TransformOperations& other) const;

  // Returns true if these operations can be blended. It will only return
  // false if we must resort to matrix interpolation, and matrix interpolation
  // fails (this can happen if either matrix cannot be decomposed).
  bool CanBlendWith(const TransformOperations& other) const;

  void AppendTranslate(double x, double y, double z);
  void AppendRotate(double x, double y, double z, double degrees);
  void AppendScale(double x, double y, double z);
  void AppendSkew(double x, double y);
  void AppendPerspective(double depth);
  void AppendMatrix(const gfx::Transform& matrix);
  void AppendIdentity();
  bool IsIdentity() const;

 private:
  bool BlendInternal(const TransformOperations& from, double progress,
                     gfx::Transform* result) const;

  std::vector<TransformOperation> operations_;

  bool ComputeDecomposedTransform() const;

  // For efficiency, we cache the decomposed transform.
  mutable scoped_ptr<gfx::DecomposedTransform> decomposed_transform_;
  mutable bool decomposed_transform_dirty_;

  DISALLOW_ASSIGN(TransformOperations);
};

}  // namespace cc

#endif  // CC_ANIMATION_TRANSFORM_OPERATIONS_H_
