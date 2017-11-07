// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_OPTIMIZATION_GUIDE_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_OPTIMIZATION_GUIDE_H_

#include "components/previews/core/previews_experiments.h"

namespace net {
class URLRequest;
}

namespace previews {

class PreviewsOptimizationGuide {
 public:
  virtual ~PreviewsOptimizationGuide() {}

  // Returns whether |type| is whitelisted for |request|.
  virtual bool IsWhitelisted(const net::URLRequest& request,
                             PreviewsType type) const = 0;

 protected:
  PreviewsOptimizationGuide() {}
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_OPTIMIZATION_GUIDE_H_
