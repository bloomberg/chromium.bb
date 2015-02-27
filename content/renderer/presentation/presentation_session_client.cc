// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/presentation/presentation_session_client.h"

#include "base/logging.h"
#include "content/renderer/presentation/presentation_session_dispatcher.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

PresentationSessionClient::PresentationSessionClient(
    PresentationSessionDispatcher* dispatcher)
    : url_(blink::WebString::fromUTF8(dispatcher->GetUrl())),
      id_(blink::WebString::fromUTF8(dispatcher->GetId())) {
}

PresentationSessionClient::~PresentationSessionClient() {
}

blink::WebString PresentationSessionClient::getUrl() {
    return url_;
}

blink::WebString PresentationSessionClient::getId() {
    return id_;
}

}  // namespace content
