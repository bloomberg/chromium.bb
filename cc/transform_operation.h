// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TRANSFORM_OPERATION_H_
#define CC_TRANSFORM_OPERATION_H

#include "third_party/WebKit/Source/Platform/chromium/public/WebTransformationMatrix.h"

namespace cc {

struct TransformOperation {
  enum Type {
    TransformOperationTranslate,
    TransformOperationRotate,
    TransformOperationScale,
    TransformOperationSkew,
    TransformOperationPerspective,
    TransformOperationMatrix,
    TransformOperationIdentity
  };

  TransformOperation()
      : type(TransformOperationIdentity) {
  }

  Type type;
  WebKit::WebTransformationMatrix matrix;

  union {
    double perspective_depth;

    struct {
      double x, y;
    } skew;

    struct {
      double x, y, z;
    } scale;

    struct {
      double x, y, z;
    } translate;

    struct {
      struct {
        double x, y, z;
      } axis;

      double angle;
    } rotate;
  };

  bool IsIdentity() const;
  static bool BlendTransformOperations(const TransformOperation* from,
                                       const TransformOperation* to,
                                       double progress,
                                       WebKit::WebTransformationMatrix& result);
};

}  // namespace cc

#endif  // CC_TRANSFORM_OPERATION_H_
