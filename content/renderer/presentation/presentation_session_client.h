// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PRESENTATION_PRESENTATION_SESSION_CLIENT_H_
#define CONTENT_RENDERER_PRESENTATION_PRESENTATION_SESSION_CLIENT_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationSessionClient.h"

namespace content {

class PresentationSessionDispatcher;

// PresentationSessionClient is passed to the Blink layer if presentation
// session has been created successfully. Owned by the callback.
class CONTENT_EXPORT PresentationSessionClient
    : public NON_EXPORTED_BASE(blink::WebPresentationSessionClient) {
 public:
  explicit PresentationSessionClient(PresentationSessionDispatcher* dispatcher);
  ~PresentationSessionClient() override;

  // WebPresentationSessionClient implementation.
  virtual blink::WebString getUrl();
  virtual blink::WebString getId();

 private:
  blink::WebString url_;
  blink::WebString id_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PRESENTATION_PRESENTATION_SESSION_CLIENT_H_
