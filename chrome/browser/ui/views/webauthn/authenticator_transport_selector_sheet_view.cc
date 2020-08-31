// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_transport_selector_sheet_view.h"

#include <utility>

#include "chrome/browser/webauthn/authenticator_transport.h"
#include "device/fido/features.h"

AuthenticatorTransportSelectorSheetView::
    AuthenticatorTransportSelectorSheetView(
        std::unique_ptr<AuthenticatorTransportSelectorSheetModel> model)
    : AuthenticatorRequestSheetView(std::move(model)) {}

AuthenticatorTransportSelectorSheetView::
    ~AuthenticatorTransportSelectorSheetView() = default;

std::unique_ptr<views::View>
AuthenticatorTransportSelectorSheetView::BuildStepSpecificContent() {
  if (base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport)) {
    auto* const dialog_model = model()->dialog_model();
    return std::make_unique<HoverListView>(
        std::make_unique<TransportHoverListModel2>(
            dialog_model->available_transports(),
            dialog_model->cable_extension_provided(),
            dialog_model->win_native_api_enabled(), model()));
  }

  return std::make_unique<HoverListView>(
      std::make_unique<TransportHoverListModel>(
          model()->dialog_model()->available_transports(), model()));
}
