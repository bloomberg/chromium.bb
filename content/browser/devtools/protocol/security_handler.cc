// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/security_handler.h"

#include <string>

#include "content/browser/devtools/devtools_session.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {
namespace protocol {

using Explanations = protocol::Array<Security::SecurityStateExplanation>;

namespace {

std::string SecurityStyleToProtocolSecurityState(
    blink::WebSecurityStyle security_style) {
  switch (security_style) {
    case blink::WebSecurityStyleUnknown:
      return Security::SecurityStateEnum::Unknown;
    case blink::WebSecurityStyleUnauthenticated:
      return Security::SecurityStateEnum::Neutral;
    case blink::WebSecurityStyleAuthenticationBroken:
      return Security::SecurityStateEnum::Insecure;
    case blink::WebSecurityStyleWarning:
      return Security::SecurityStateEnum::Warning;
    case blink::WebSecurityStyleAuthenticated:
      return Security::SecurityStateEnum::Secure;
    default:
      NOTREACHED();
      return Security::SecurityStateEnum::Unknown;
  }
}

void AddExplanations(
    const std::string& security_style,
    const std::vector<SecurityStyleExplanation>& explanations_to_add,
    Explanations* explanations) {
  for (const auto& it : explanations_to_add) {
    explanations->addItem(Security::SecurityStateExplanation::Create()
        .SetSecurityState(security_style)
        .SetSummary(it.summary)
        .SetDescription(it.description)
        .SetHasCertificate(it.has_certificate)
        .Build());
  }
}

}  // namespace

// static
SecurityHandler* SecurityHandler::FromAgentHost(DevToolsAgentHostImpl* host) {
  DevToolsSession* session = DevToolsDomainHandler::GetFirstSession(host);
  if (!session)
    return nullptr;
  return static_cast<SecurityHandler*>(
      session->GetHandlerByName(Security::Metainfo::domainName));
}

SecurityHandler::SecurityHandler()
    : DevToolsDomainHandler(Security::Metainfo::domainName),
      enabled_(false),
      host_(nullptr) {
}

SecurityHandler::~SecurityHandler() {
}

void SecurityHandler::Wire(UberDispatcher* dispatcher) {
  frontend_.reset(new Security::Frontend(dispatcher->channel()));
  Security::Dispatcher::wire(dispatcher, this);
}

void SecurityHandler::AttachToRenderFrameHost() {
  DCHECK(host_);
  WebContents* web_contents = WebContents::FromRenderFrameHost(host_);
  WebContentsObserver::Observe(web_contents);

  // Send an initial DidChangeVisibleSecurityState event.
  DCHECK(enabled_);
  DidChangeVisibleSecurityState();
}

void SecurityHandler::SetRenderFrameHost(RenderFrameHostImpl* host) {
  host_ = host;
  if (enabled_ && host_)
    AttachToRenderFrameHost();
}

void SecurityHandler::DidChangeVisibleSecurityState() {
  DCHECK(enabled_);

  SecurityStyleExplanations security_style_explanations;
  blink::WebSecurityStyle security_style =
      web_contents()->GetDelegate()->GetSecurityStyle(
          web_contents(), &security_style_explanations);

  const std::string security_state =
      SecurityStyleToProtocolSecurityState(security_style);

  std::unique_ptr<Explanations> explanations = Explanations::create();
  AddExplanations(Security::SecurityStateEnum::Insecure,
                  security_style_explanations.broken_explanations,
                  explanations.get());
  AddExplanations(Security::SecurityStateEnum::Neutral,
                  security_style_explanations.unauthenticated_explanations,
                  explanations.get());
  AddExplanations(Security::SecurityStateEnum::Secure,
                  security_style_explanations.secure_explanations,
                  explanations.get());
  AddExplanations(Security::SecurityStateEnum::Info,
                  security_style_explanations.info_explanations,
                  explanations.get());

  std::unique_ptr<Security::InsecureContentStatus> insecure_status =
      Security::InsecureContentStatus::Create()
          .SetRanMixedContent(security_style_explanations.ran_mixed_content)
          .SetDisplayedMixedContent(
              security_style_explanations.displayed_mixed_content)
          .SetRanContentWithCertErrors(
              security_style_explanations.ran_content_with_cert_errors)
          .SetDisplayedContentWithCertErrors(
              security_style_explanations.displayed_content_with_cert_errors)
          .SetRanInsecureContentStyle(SecurityStyleToProtocolSecurityState(
              security_style_explanations.ran_insecure_content_style))
          .SetDisplayedInsecureContentStyle(
              SecurityStyleToProtocolSecurityState(
                  security_style_explanations
                      .displayed_insecure_content_style))
          .Build();

  frontend_->SecurityStateChanged(
      security_state,
      security_style_explanations.scheme_is_cryptographic,
      std::move(explanations),
      std::move(insecure_status),
      Maybe<std::string>(security_style_explanations.summary));
}

Response SecurityHandler::Enable() {
  enabled_ = true;
  if (host_)
    AttachToRenderFrameHost();

  return Response::OK();
}

Response SecurityHandler::Disable() {
  enabled_ = false;
  WebContentsObserver::Observe(nullptr);
  return Response::OK();
}

Response SecurityHandler::ShowCertificateViewer() {
  if (!host_)
    return Response::InternalError();
  WebContents* web_contents = WebContents::FromRenderFrameHost(host_);
  scoped_refptr<net::X509Certificate> certificate =
      web_contents->GetController().GetVisibleEntry()->GetSSL().certificate;
  if (!certificate)
    return Response::Error("Could not find certificate");
  web_contents->GetDelegate()->ShowCertificateViewerInDevTools(
      web_contents, certificate);
  return Response::OK();
}

}  // namespace protocol
}  // namespace content
