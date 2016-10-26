// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/new_window_client_proxy.h"

#include "base/logging.h"
#include "services/service_manager/public/cpp/connector.h"

namespace ash {

NewWindowClientProxy::NewWindowClientProxy(
    service_manager::Connector* connector)
    : connector_(connector) {}

NewWindowClientProxy::~NewWindowClientProxy() {}

void NewWindowClientProxy::NewTab() {
  EnsureInterface();
  client_->NewTab();
}

void NewWindowClientProxy::NewWindow(bool incognito) {
  EnsureInterface();
  client_->NewWindow(incognito);
}

void NewWindowClientProxy::OpenFileManager() {
  EnsureInterface();
  client_->OpenFileManager();
}

void NewWindowClientProxy::OpenCrosh() {
  EnsureInterface();
  client_->OpenCrosh();
}

void NewWindowClientProxy::OpenGetHelp() {
  EnsureInterface();
  client_->OpenGetHelp();
}

void NewWindowClientProxy::RestoreTab() {
  EnsureInterface();
  client_->RestoreTab();
}

void NewWindowClientProxy::ShowKeyboardOverlay() {
  EnsureInterface();
  client_->ShowKeyboardOverlay();
}

void NewWindowClientProxy::ShowTaskManager() {
  EnsureInterface();
  client_->ShowTaskManager();
}

void NewWindowClientProxy::OpenFeedbackPage() {
  EnsureInterface();
  client_->OpenFeedbackPage();
}

void NewWindowClientProxy::EnsureInterface() {
  // |connector_| can be null in unit tests. We check this at first usage
  // instead of during construction because a NewWindowClientProxy is always
  // created and is then replaced with a mock in the unit tests.
  DCHECK(connector_);

  if (client_)
    return;
  connector_->ConnectToInterface("service:content_browser", &client_);
  client_.set_connection_error_handler(base::Bind(
      &NewWindowClientProxy::OnClientConnectionError, base::Unretained(this)));
}

void NewWindowClientProxy::OnClientConnectionError() {
  client_.reset();
}

}  // namespace ash
