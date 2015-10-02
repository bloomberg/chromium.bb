// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PRESENTATION_PRESENTATION_SESSION_CLIENT_H_
#define CONTENT_RENDERER_PRESENTATION_PRESENTATION_SESSION_CLIENT_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/common/presentation/presentation_service.mojom.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnectionClient.h"

namespace content {

// PresentationConnectionClient is passed to the Blink layer if presentation
// session has been created successfully. Owned by the callback.
class CONTENT_EXPORT PresentationConnectionClient
    : public NON_EXPORTED_BASE(blink::WebPresentationConnectionClient) {
 public:
  explicit PresentationConnectionClient(
        presentation::PresentationSessionInfoPtr session_info);
  explicit PresentationConnectionClient(const mojo::String& url,
                                     const mojo::String& id);
  ~PresentationConnectionClient() override;

  // WebPresentationConnectionClient implementation.
  blink::WebString getUrl() override;
  blink::WebString getId() override;

 private:
  blink::WebString url_;
  blink::WebString id_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PRESENTATION_PRESENTATION_SESSION_CLIENT_H_
