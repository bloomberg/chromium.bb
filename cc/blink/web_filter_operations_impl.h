// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_FILTER_OPERATIONS_IMPL_H_
#define CC_BLINK_WEB_FILTER_OPERATIONS_IMPL_H_

#include "cc/output/filter_operations.h"
#include "cc/blink/cc_blink_export.h"
#include "third_party/WebKit/public/platform/WebFilterOperations.h"

namespace cc_blink {

class WebFilterOperationsImpl : public blink::WebFilterOperations {
 public:
  CC_BLINK_EXPORT WebFilterOperationsImpl();
  virtual ~WebFilterOperationsImpl();

  const cc::FilterOperations& AsFilterOperations() const;

  // Implementation of blink::WebFilterOperations methods
  virtual void appendGrayscaleFilter(float amount);
  virtual void appendSepiaFilter(float amount);
  virtual void appendSaturateFilter(float amount);
  virtual void appendHueRotateFilter(float amount);
  virtual void appendInvertFilter(float amount);
  virtual void appendBrightnessFilter(float amount);
  virtual void appendContrastFilter(float amount);
  virtual void appendOpacityFilter(float amount);
  virtual void appendBlurFilter(float amount);
  virtual void appendDropShadowFilter(blink::WebPoint offset,
                                      float std_deviation,
                                      blink::WebColor color);
  virtual void appendColorMatrixFilter(SkScalar matrix[20]);
  virtual void appendZoomFilter(float amount, int inset);
  virtual void appendSaturatingBrightnessFilter(float amount);
  virtual void appendReferenceFilter(SkImageFilter* image_filter);

  virtual void clear();

 private:
  cc::FilterOperations filter_operations_;

  DISALLOW_COPY_AND_ASSIGN(WebFilterOperationsImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_FILTER_OPERATIONS_IMPL_H_
