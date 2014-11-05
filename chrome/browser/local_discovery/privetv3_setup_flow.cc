// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privetv3_setup_flow.h"

#include "base/logging.h"
#include "chrome/browser/local_discovery/gcd_registration_ticket_request.h"

namespace local_discovery {

namespace {

const char kTicketJsonKeyName[] = "registration.ticketID";
const char kUserJsonKeyName[] = "registration.user";

}  // namespace

PrivetV3SetupFlow::Delegate::~Delegate() {
}

PrivetV3SetupFlow::PrivetV3SetupFlow(Delegate* delegate)
    : delegate_(delegate), weak_ptr_factory_(this) {
}

PrivetV3SetupFlow::~PrivetV3SetupFlow() {
}

void PrivetV3SetupFlow::Register(const std::string& service_name) {
  service_name_ = service_name;
  ticket_request_ = delegate_->CreateApiFlow();
  if (!ticket_request_) {
    OnSetupError();
    return;
  }
  scoped_ptr<GCDApiFlow::Request> ticket_request(
      new GCDRegistrationTicketRequest(
          base::Bind(&PrivetV3SetupFlow::OnTicketCreated,
                     weak_ptr_factory_.GetWeakPtr())));
  ticket_request_->Start(ticket_request.Pass());
}

#if defined(ENABLE_WIFI_BOOTSTRAPPING)
void PrivetV3SetupFlow::SetupWifiAndRegister(const std::string& device_ssid) {
  NOTIMPLEMENTED();
}
#endif  // ENABLE_WIFI_BOOTSTRAPPING

void PrivetV3SetupFlow::OnSetupError() {
  delegate_->OnSetupError();
}

void PrivetV3SetupFlow::OnTicketCreated(const std::string& ticket_id,
                                        const std::string& device_id) {
  if (ticket_id.empty() || device_id.empty()) {
    OnSetupError();
    return;
  }
  // TODO(vitalybuka): Implement success check by polling status of device_id_.
  device_id_ = device_id;
  ticket_id_ = ticket_id;
  delegate_->CreatePrivetV3Client(
      service_name_,
      base::Bind(&PrivetV3SetupFlow::OnPrivetClientCreated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PrivetV3SetupFlow::OnPrivetClientCreated(
    scoped_ptr<PrivetHTTPClient> privet_http_client) {
  if (!privet_http_client) {
    OnSetupError();
    return;
  }
  session_.reset(new PrivetV3Session(privet_http_client.Pass()));
  session_->Init(base::Bind(&PrivetV3SetupFlow::OnSessionInitialized,
                            weak_ptr_factory_.GetWeakPtr()));
}

void PrivetV3SetupFlow::OnCodeConfirmed(bool success) {
  if (!success)
    return OnSetupError();
  session_->ConfirmCode("1234", base::Bind(&PrivetV3SetupFlow::OnPairingDone,
                                           weak_ptr_factory_.GetWeakPtr()));
}

void PrivetV3SetupFlow::OnSessionInitialized(
    PrivetV3Session::Result result,
    const std::vector<PrivetV3Session::PairingType>& types) {
  if (result != PrivetV3Session::Result::STATUS_SUCCESS)
    return OnSetupError();
  session_->StartPairing(PrivetV3Session::PairingType::PAIRING_TYPE_PINCODE,
                         base::Bind(&PrivetV3SetupFlow::OnPairingStarted,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void PrivetV3SetupFlow::OnPairingStarted(PrivetV3Session::Result result) {
  if (result != PrivetV3Session::Result::STATUS_SUCCESS)
    return OnSetupError();
  delegate_->ConfirmSecurityCode(base::Bind(&PrivetV3SetupFlow::OnCodeConfirmed,
                                            weak_ptr_factory_.GetWeakPtr()));
}

void PrivetV3SetupFlow::OnPairingDone(PrivetV3Session::Result result) {
  if (result != PrivetV3Session::Result::STATUS_SUCCESS)
    return OnSetupError();
  base::DictionaryValue message;
  message.SetString(kTicketJsonKeyName, ticket_id_);
  message.SetString(kUserJsonKeyName, "me");
  session_->SendMessage("/privet/v3/setup/start", message,
                        base::Bind(&PrivetV3SetupFlow::OnSetupMessageSent,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void PrivetV3SetupFlow::OnSetupMessageSent(
    PrivetV3Session::Result result,
    const base::DictionaryValue& response) {
  if (result != PrivetV3Session::Result::STATUS_SUCCESS)
    return OnSetupError();
  delegate_->OnSetupDone();
}

}  // namespace local_discovery
