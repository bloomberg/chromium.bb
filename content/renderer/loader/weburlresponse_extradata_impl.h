// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_WEBURLRESPONSE_EXTRADATA_IMPL_H_
#define CONTENT_RENDERER_LOADER_WEBURLRESPONSE_EXTRADATA_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "net/http/http_response_info.h"
#include "net/nqe/effective_connection_type.h"
#include "third_party/blink/public/platform/web_url_response.h"

namespace content {

class CONTENT_EXPORT WebURLResponseExtraDataImpl
    : public blink::WebURLResponse::ExtraData {
 public:
  WebURLResponseExtraDataImpl();
  ~WebURLResponseExtraDataImpl() override;

  net::EffectiveConnectionType effective_connection_type() const {
    return effective_connection_type_;
  }
  void set_effective_connection_type(
      net::EffectiveConnectionType effective_connection_type) {
    effective_connection_type_ = effective_connection_type;
  }

 private:
  net::EffectiveConnectionType effective_connection_type_;

  DISALLOW_COPY_AND_ASSIGN(WebURLResponseExtraDataImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_WEBURLRESPONSE_EXTRADATA_IMPL_H_
