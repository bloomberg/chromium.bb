// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/renderer/credential_manager_client.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/WebKit/public/platform/WebCredential.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerError.h"
#include "third_party/WebKit/public/platform/WebFederatedCredential.h"
#include "third_party/WebKit/public/platform/WebPasswordCredential.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace password_manager {

namespace {

void WebCredentialToCredentialInfo(const blink::WebCredential& credential,
                                   CredentialInfo* out) {
  out->id = credential.Id().Utf16();
  if (credential.IsPasswordCredential()) {
    out->type = CredentialType::CREDENTIAL_TYPE_PASSWORD;
    out->password = static_cast<const blink::WebPasswordCredential&>(credential)
                        .Password()
                        .Utf16();
    out->name = static_cast<const blink::WebPasswordCredential&>(credential)
                    .Name()
                    .Utf16();
    out->icon =
        static_cast<const blink::WebPasswordCredential&>(credential).IconURL();
  } else {
    DCHECK(credential.IsFederatedCredential());
    out->type = CredentialType::CREDENTIAL_TYPE_FEDERATED;
    out->federation =
        static_cast<const blink::WebFederatedCredential&>(credential)
            .Provider();
    out->name = static_cast<const blink::WebFederatedCredential&>(credential)
                    .Name()
                    .Utf16();
    out->icon =
        static_cast<const blink::WebFederatedCredential&>(credential).IconURL();
  }
}

std::unique_ptr<blink::WebCredential> CredentialInfoToWebCredential(
    const CredentialInfo& info) {
  switch (info.type) {
    case CredentialType::CREDENTIAL_TYPE_FEDERATED:
      return std::make_unique<blink::WebFederatedCredential>(
          blink::WebString::FromUTF16(info.id), info.federation,
          blink::WebString::FromUTF16(info.name), info.icon);
    case CredentialType::CREDENTIAL_TYPE_PASSWORD:
      return std::make_unique<blink::WebPasswordCredential>(
          blink::WebString::FromUTF16(info.id),
          blink::WebString::FromUTF16(info.password),
          blink::WebString::FromUTF16(info.name), info.icon);
    case CredentialType::CREDENTIAL_TYPE_EMPTY:
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

CredentialMediationRequirement GetCredentialMediationRequirementFromBlink(
    blink::WebCredentialMediationRequirement mediation) {
  switch (mediation) {
    case blink::WebCredentialMediationRequirement::kSilent:
      return CredentialMediationRequirement::kSilent;
    case blink::WebCredentialMediationRequirement::kOptional:
      return CredentialMediationRequirement::kOptional;
    case blink::WebCredentialMediationRequirement::kRequired:
      return CredentialMediationRequirement::kRequired;
  }

  NOTREACHED();
  return CredentialMediationRequirement::kOptional;
}

blink::WebCredentialManagerError GetWebCredentialManagerErrorFromMojo(
    CredentialManagerError error) {
  switch (error) {
    case CredentialManagerError::DISABLED:
      return blink::WebCredentialManagerError::
          kWebCredentialManagerDisabledError;
    case CredentialManagerError::PENDINGREQUEST:
      return blink::WebCredentialManagerError::
          kWebCredentialManagerPendingRequestError;
    case CredentialManagerError::PASSWORDSTOREUNAVAILABLE:
      return blink::WebCredentialManagerError::
          kWebCredentialManagerPasswordStoreUnavailableError;
    case CredentialManagerError::UNKNOWN:
      return blink::WebCredentialManagerError::
          kWebCredentialManagerUnknownError;
    case CredentialManagerError::SUCCESS:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return blink::WebCredentialManagerError::kWebCredentialManagerUnknownError;
}

// Takes ownership of blink::WebCredentialManagerClient::NotificationCallbacks
// pointer. When the wrapper is destroyed, if |callbacks| is still alive
// its onError() will get called.
class NotificationCallbacksWrapper {
 public:
  explicit NotificationCallbacksWrapper(
      blink::WebCredentialManagerClient::NotificationCallbacks* callbacks);

  ~NotificationCallbacksWrapper();

  void NotifySuccess();

 private:
  std::unique_ptr<blink::WebCredentialManagerClient::NotificationCallbacks>
      callbacks_;

  DISALLOW_COPY_AND_ASSIGN(NotificationCallbacksWrapper);
};

NotificationCallbacksWrapper::NotificationCallbacksWrapper(
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks)
    : callbacks_(callbacks) {}

NotificationCallbacksWrapper::~NotificationCallbacksWrapper() {
  if (callbacks_)
    callbacks_->OnError(blink::kWebCredentialManagerUnknownError);
}

void NotificationCallbacksWrapper::NotifySuccess() {
  // Call onSuccess() and reset callbacks to avoid calling onError() in
  // destructor.
  if (callbacks_) {
    callbacks_->OnSuccess();
    callbacks_.reset();
  }
}

// Takes ownership of blink::WebCredentialManagerClient::RequestCallbacks
// pointer. When the wrapper is destroied, if |callbacks| is still alive
// its onError() will get called.
class RequestCallbacksWrapper {
 public:
  explicit RequestCallbacksWrapper(
      blink::WebCredentialManagerClient::RequestCallbacks* callbacks);

  ~RequestCallbacksWrapper();

  void NotifySuccess(const CredentialInfo& info);

  void NotifyError(CredentialManagerError error);

 private:
  std::unique_ptr<blink::WebCredentialManagerClient::RequestCallbacks>
      callbacks_;

  DISALLOW_COPY_AND_ASSIGN(RequestCallbacksWrapper);
};

RequestCallbacksWrapper::RequestCallbacksWrapper(
    blink::WebCredentialManagerClient::RequestCallbacks* callbacks)
    : callbacks_(callbacks) {}

RequestCallbacksWrapper::~RequestCallbacksWrapper() {
  if (callbacks_)
    callbacks_->OnError(blink::kWebCredentialManagerUnknownError);
}

void RequestCallbacksWrapper::NotifySuccess(const CredentialInfo& info) {
  // Call onSuccess() and reset callbacks to avoid calling onError() in
  // destructor.
  if (callbacks_) {
    callbacks_->OnSuccess(CredentialInfoToWebCredential(info));
    callbacks_.reset();
  }
}

void RequestCallbacksWrapper::NotifyError(CredentialManagerError error) {
  if (callbacks_) {
    callbacks_->OnError(GetWebCredentialManagerErrorFromMojo(error));
    callbacks_.reset();
  }
}

void RespondToNotificationCallback(
    NotificationCallbacksWrapper* callbacks_wrapper) {
  callbacks_wrapper->NotifySuccess();
}

void RespondToRequestCallback(RequestCallbacksWrapper* callbacks_wrapper,
                              CredentialManagerError error,
                              const base::Optional<CredentialInfo>& info) {
  if (error == CredentialManagerError::SUCCESS) {
    DCHECK(info);
    callbacks_wrapper->NotifySuccess(*info);
  } else {
    DCHECK(!info);
    callbacks_wrapper->NotifyError(error);
  }
}

}  // namespace

CredentialManagerClient::CredentialManagerClient(
    content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {
  render_view->GetWebView()->SetCredentialManagerClient(this);
}

CredentialManagerClient::~CredentialManagerClient() {}

// -----------------------------------------------------------------------------
// Access mojo CredentialManagerService.

void CredentialManagerClient::DispatchStore(
    const blink::WebCredential& credential,
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks) {
  DCHECK(callbacks);
  ConnectToMojoCMIfNeeded();

  CredentialInfo info;
  WebCredentialToCredentialInfo(credential, &info);
  mojo_cm_service_->Store(
      info,
      base::Bind(&RespondToNotificationCallback,
                 base::Owned(new NotificationCallbacksWrapper(callbacks))));
}

void CredentialManagerClient::DispatchPreventSilentAccess(
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks) {
  DCHECK(callbacks);
  ConnectToMojoCMIfNeeded();

  mojo_cm_service_->PreventSilentAccess(
      base::Bind(&RespondToNotificationCallback,
                 base::Owned(new NotificationCallbacksWrapper(callbacks))));
}

void CredentialManagerClient::DispatchGet(
    blink::WebCredentialMediationRequirement mediation,
    bool include_passwords,
    const blink::WebVector<blink::WebURL>& federations,
    RequestCallbacks* callbacks) {
  DCHECK(callbacks);
  ConnectToMojoCMIfNeeded();

  std::vector<GURL> federation_vector;
  for (size_t i = 0; i < std::min(federations.size(), kMaxFederations); ++i)
    federation_vector.push_back(federations[i]);

  mojo_cm_service_->Get(
      GetCredentialMediationRequirementFromBlink(mediation), include_passwords,
      federation_vector,
      base::Bind(&RespondToRequestCallback,
                 base::Owned(new RequestCallbacksWrapper(callbacks))));
}

void CredentialManagerClient::ConnectToMojoCMIfNeeded() {
  if (mojo_cm_service_)
    return;

  content::RenderFrame* main_frame = render_view()->GetMainRenderFrame();
  main_frame->GetRemoteAssociatedInterfaces()->GetInterface(&mojo_cm_service_);

  // The remote end of the pipe will be forcibly closed by the browser side
  // after each main frame navigation. Set up an error handler to reset the
  // local end so that there will be an attempt at reestablishing the connection
  // at the next call to the API.
  mojo_cm_service_.set_connection_error_handler(base::Bind(
      &CredentialManagerClient::OnMojoConnectionError, base::Unretained(this)));
}

void CredentialManagerClient::OnMojoConnectionError() {
  mojo_cm_service_.reset();
}

void CredentialManagerClient::OnDestruct() {
  delete this;
}

}  // namespace password_manager
