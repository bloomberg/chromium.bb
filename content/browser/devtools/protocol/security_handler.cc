// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/security_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "net/cert/x509_certificate.h"
#include "third_party/WebKit/public/platform/WebMixedContentContextType.h"

namespace content {
namespace protocol {

using Explanations = protocol::Array<Security::SecurityStateExplanation>;

namespace {

std::string SecurityStyleToProtocolSecurityState(
    blink::WebSecurityStyle security_style) {
  switch (security_style) {
    case blink::kWebSecurityStyleUnknown:
      return Security::SecurityStateEnum::Unknown;
    case blink::kWebSecurityStyleNeutral:
      return Security::SecurityStateEnum::Neutral;
    case blink::kWebSecurityStyleInsecure:
      return Security::SecurityStateEnum::Insecure;
    case blink::kWebSecurityStyleSecure:
      return Security::SecurityStateEnum::Secure;
    default:
      NOTREACHED();
      return Security::SecurityStateEnum::Unknown;
  }
}

std::string MixedContentTypeToProtocolMixedContentType(
    blink::WebMixedContentContextType mixed_content_type) {
  switch (mixed_content_type) {
    case blink::WebMixedContentContextType::kNotMixedContent:
      return Security::MixedContentTypeEnum::None;
    case blink::WebMixedContentContextType::kBlockable:
      return Security::MixedContentTypeEnum::Blockable;
    case blink::WebMixedContentContextType::kOptionallyBlockable:
      return Security::MixedContentTypeEnum::OptionallyBlockable;
    case blink::WebMixedContentContextType::kShouldBeBlockable:
      // kShouldBeBlockable is not used for explanations.
      NOTREACHED();
      return Security::MixedContentTypeEnum::OptionallyBlockable;
    default:
      NOTREACHED();
      return Security::MixedContentTypeEnum::None;
  }
}

void AddExplanations(
    const std::string& security_style,
    const std::vector<SecurityStyleExplanation>& explanations_to_add,
    Explanations* explanations) {
  for (const auto& it : explanations_to_add) {
    std::unique_ptr<protocol::Array<String>> certificate =
        protocol::Array<String>::create();
    if (it.certificate) {
      std::string der;
      std::string encoded;
      bool rv = net::X509Certificate::GetDEREncoded(
          it.certificate->os_cert_handle(), &der);
      DCHECK(rv);
      base::Base64Encode(der, &encoded);

      certificate->addItem(encoded);

      for (auto* cert : it.certificate->GetIntermediateCertificates()) {
        rv = net::X509Certificate::GetDEREncoded(cert, &der);
        DCHECK(rv);
        base::Base64Encode(der, &encoded);
        certificate->addItem(encoded);
      }
    }

    explanations->addItem(
        Security::SecurityStateExplanation::Create()
            .SetSecurityState(security_style)
            .SetSummary(it.summary)
            .SetDescription(it.description)
            .SetCertificate(std::move(certificate))
            .SetMixedContentType(MixedContentTypeToProtocolMixedContentType(
                it.mixed_content_type))
            .Build());
  }
}

}  // namespace

// static
std::vector<SecurityHandler*> SecurityHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return DevToolsSession::HandlersForAgentHost<SecurityHandler>(
      host, Security::Metainfo::domainName);
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

void SecurityHandler::SetRenderer(RenderProcessHost* process_host,
                                  RenderFrameHostImpl* frame_host) {
  host_ = frame_host;
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
                  security_style_explanations.insecure_explanations,
                  explanations.get());
  AddExplanations(Security::SecurityStateEnum::Neutral,
                  security_style_explanations.neutral_explanations,
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
          .SetContainedMixedForm(
              security_style_explanations.contained_mixed_form)
          .SetRanContentWithCertErrors(
              security_style_explanations.ran_content_with_cert_errors)
          .SetDisplayedContentWithCertErrors(
              security_style_explanations.displayed_content_with_cert_errors)
          .SetRanInsecureContentStyle(SecurityStyleToProtocolSecurityState(
              security_style_explanations.ran_insecure_content_style))
          .SetDisplayedInsecureContentStyle(
              SecurityStyleToProtocolSecurityState(
                  security_style_explanations.displayed_insecure_content_style))
          .Build();

  frontend_->SecurityStateChanged(
      security_state,
      security_style_explanations.scheme_is_cryptographic,
      std::move(explanations),
      std::move(insecure_status),
      Maybe<std::string>(security_style_explanations.summary));
}

void SecurityHandler::DidFinishNavigation(NavigationHandle* navigation_handle) {
  if (certificate_errors_overriden_)
    FlushPendingCertificateErrorNotifications();
}

void SecurityHandler::FlushPendingCertificateErrorNotifications() {
  for (auto callback : cert_error_callbacks_)
    callback.second.Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
  cert_error_callbacks_.clear();
}

bool SecurityHandler::NotifyCertificateError(int cert_error,
                                             const GURL& request_url,
                                             CertErrorCallback handler) {
  if (!enabled_)
    return false;
  frontend_->CertificateError(++last_cert_error_id_,
                              net::ErrorToShortString(cert_error),
                              request_url.spec());
  if (!certificate_errors_overriden_) {
    return false;
  }
  cert_error_callbacks_[last_cert_error_id_] = handler;
  return true;
}

Response SecurityHandler::Enable() {
  enabled_ = true;
  if (host_)
    AttachToRenderFrameHost();

  return Response::OK();
}

Response SecurityHandler::Disable() {
  enabled_ = false;
  certificate_errors_overriden_ = false;
  WebContentsObserver::Observe(nullptr);
  FlushPendingCertificateErrorNotifications();
  return Response::OK();
}

Response SecurityHandler::HandleCertificateError(int event_id,
                                                 const String& action) {
  if (cert_error_callbacks_.find(event_id) == cert_error_callbacks_.end()) {
    return Response::Error(
        String("Unknown event id: " + std::to_string(event_id)));
  }
  content::CertificateRequestResultType type =
      content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
  Response response = Response::OK();
  if (action == Security::CertificateErrorActionEnum::Continue) {
    type = content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE;
  } else if (action == Security::CertificateErrorActionEnum::Cancel) {
    type = content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
  } else {
    response =
        Response::Error(String("Unknown Certificate Error Action: " + action));
  }
  cert_error_callbacks_[event_id].Run(type);
  cert_error_callbacks_.erase(event_id);
  return response;
}

Response SecurityHandler::SetOverrideCertificateErrors(bool override) {
  if (override && !enabled_)
    return Response::Error("Security domain not enabled");
  certificate_errors_overriden_ = override;
  if (!override)
    FlushPendingCertificateErrorNotifications();
  return Response::OK();
}

}  // namespace protocol
}  // namespace content
