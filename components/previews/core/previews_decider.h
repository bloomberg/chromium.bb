// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_DECIDER_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_DECIDER_H_

#include "components/previews/core/previews_experiments.h"

#include "net/nqe/effective_connection_type.h"

namespace net {
class URLRequest;
}

namespace previews {

class PreviewsDecider {
 public:
  // Whether |request| is allowed to show a preview of |type|. If the current
  // ECT is strictly faster than |effective_connection_type_threshold|, the
  // preview will be disallowed; preview types that check network quality before
  // calling ShouldAllowPreviewAtECT should pass in
  // EFFECTIVE_CONNECTION_TYPE_4G.
  virtual bool ShouldAllowPreviewAtECT(
      const net::URLRequest& request,
      PreviewsType type,
      net::EffectiveConnectionType effective_connection_type_threshold)
      const = 0;

  // Same as ShouldAllowPreviewAtECT, but uses the previews default
  // EffectiveConnectionType.
  virtual bool ShouldAllowPreview(const net::URLRequest& request,
                                  PreviewsType type) const = 0;

 protected:
  PreviewsDecider() {}
  virtual ~PreviewsDecider() {}
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_DECIDER_H_
