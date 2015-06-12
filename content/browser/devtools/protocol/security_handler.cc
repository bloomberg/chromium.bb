// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/security_handler.h"

#include <string>

#include "content/public/browser/web_contents.h"

namespace content {
namespace devtools {
namespace security {

typedef DevToolsProtocolClient::Response Response;

namespace {

std::string SecurityStyleToProtocolSecurityState(
    SecurityStyle security_style) {
  switch (security_style) {
    case SECURITY_STYLE_UNKNOWN:
      return kSecurityStateUnknown;
    case SECURITY_STYLE_UNAUTHENTICATED:
      return kSecurityStateHttp;
    case SECURITY_STYLE_AUTHENTICATION_BROKEN:
      return kSecurityStateInsecure;
    case SECURITY_STYLE_WARNING:
      return kSecurityStateWarning;
    case SECURITY_STYLE_AUTHENTICATED:
      return kSecurityStateSecure;
    default:
      NOTREACHED();
      return kSecurityStateUnknown;
  }
}

}  // namespace

SecurityHandler::SecurityHandler()
    : enabled_(false),
      host_(nullptr) {
}

SecurityHandler::~SecurityHandler() {
}

void SecurityHandler::SetClient(scoped_ptr<Client> client) {
  client_.swap(client);
}

void SecurityHandler::SetRenderFrameHost(RenderFrameHost* host) {
  host_ = host;
  if (enabled_ && host_)
    WebContentsObserver::Observe(WebContents::FromRenderFrameHost(host_));
}

void SecurityHandler::SecurityStyleChanged(SecurityStyle security_style) {
  DCHECK(enabled_);

  const std::string security_state =
      SecurityStyleToProtocolSecurityState(security_style);
  client_->SecurityStateChanged(
      SecurityStateChangedParams::Create()->set_security_state(security_state));
}

Response SecurityHandler::Enable() {
  enabled_ = true;
  if (host_)
    WebContentsObserver::Observe(WebContents::FromRenderFrameHost(host_));
  return Response::OK();
}

Response SecurityHandler::Disable() {
  enabled_ = false;
  WebContentsObserver::Observe(nullptr);
  return Response::OK();
}

}  // namespace security
}  // namespace devtools
}  // namespace content
