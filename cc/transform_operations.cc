// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/transform_operations.h"

using WebKit::WebTransformationMatrix;

namespace cc {

TransformOperations::TransformOperations() {
}

TransformOperations::TransformOperations(const TransformOperations& other) {
  operations_ = other.operations_;
}

TransformOperations::~TransformOperations() {
}

WebTransformationMatrix TransformOperations::Apply() const {
  WebTransformationMatrix to_return;
  for (size_t i = 0; i < operations_.size(); ++i)
    to_return.multiply(operations_[i].matrix);
  return to_return;
}

WebTransformationMatrix TransformOperations::Blend(
    const TransformOperations& from, double progress) const {
  WebTransformationMatrix to_return;
  BlendInternal(from, progress, to_return);
  return to_return;
}

bool TransformOperations::MatchesTypes(const TransformOperations& other) const {
  if (IsIdentity() || other.IsIdentity())
    return true;

  if (operations_.size() != other.operations_.size())
    return false;

  for (size_t i = 0; i < operations_.size(); ++i) {
    if (operations_[i].type != other.operations_[i].type
      && !operations_[i].IsIdentity()
      && !other.operations_[i].IsIdentity())
      return false;
  }

  return true;
}

bool TransformOperations::CanBlendWith(
    const TransformOperations& other) const {
  WebTransformationMatrix dummy;
  return BlendInternal(other, 0.5, dummy);
}

void TransformOperations::AppendTranslate(double x, double y, double z) {
  TransformOperation to_add;
  to_add.matrix.translate3d(x, y, z);
  to_add.type = TransformOperation::TransformOperationTranslate;
  to_add.translate.x = x;
  to_add.translate.y = y;
  to_add.translate.z = z;
  operations_.push_back(to_add);
}

void TransformOperations::AppendRotate(double x, double y, double z,
                                       double degrees) {
  TransformOperation to_add;
  to_add.matrix.rotate3d(x, y, z, degrees);
  to_add.type = TransformOperation::TransformOperationRotate;
  to_add.rotate.axis.x = x;
  to_add.rotate.axis.y = y;
  to_add.rotate.axis.z = z;
  to_add.rotate.angle = degrees;
  operations_.push_back(to_add);
}

void TransformOperations::AppendScale(double x, double y, double z) {
  TransformOperation to_add;
  to_add.matrix.scale3d(x, y, z);
  to_add.type = TransformOperation::TransformOperationScale;
  to_add.scale.x = x;
  to_add.scale.y = y;
  to_add.scale.z = z;
  operations_.push_back(to_add);
}

void TransformOperations::AppendSkew(double x, double y) {
  TransformOperation to_add;
  to_add.matrix.skewX(x);
  to_add.matrix.skewY(y);
  to_add.type = TransformOperation::TransformOperationSkew;
  to_add.skew.x = x;
  to_add.skew.y = y;
  operations_.push_back(to_add);
}

void TransformOperations::AppendPerspective(double depth) {
  TransformOperation to_add;
  to_add.matrix.applyPerspective(depth);
  to_add.type = TransformOperation::TransformOperationPerspective;
  to_add.perspective_depth = depth;
  operations_.push_back(to_add);
}

void TransformOperations::AppendMatrix(const WebTransformationMatrix& matrix) {
  TransformOperation to_add;
  to_add.matrix = matrix;
  to_add.type = TransformOperation::TransformOperationMatrix;
  operations_.push_back(to_add);
}

void TransformOperations::AppendIdentity() {
  operations_.push_back(TransformOperation());
}

bool TransformOperations::IsIdentity() const {
  for (size_t i = 0; i < operations_.size(); ++i) {
    if (!operations_[i].IsIdentity())
      return false;
  }
  return true;
}

bool TransformOperations::BlendInternal(const TransformOperations& from,
                                        double progress,
                                        WebTransformationMatrix& result) const {
  bool from_identity = from.IsIdentity();
  bool to_identity = IsIdentity();
  if (from_identity && to_identity)
    return true;

  if (MatchesTypes(from)) {
    size_t num_operations =
        std::max(from_identity ? 0 : from.operations_.size(),
                 to_identity ? 0 : operations_.size());
    for (size_t i = 0; i < num_operations; ++i) {
      WebTransformationMatrix blended;
      if (!TransformOperation::BlendTransformOperations(
          from_identity ? 0 : &from.operations_[i],
          to_identity ? 0 : &operations_[i],
          progress,
          blended))
          return false;
      result.multiply(blended);
    }
    return true;
  }

  result = Apply();
  WebTransformationMatrix from_transform = from.Apply();
  return result.blend(from_transform, progress);
}

}  // namespace cc
