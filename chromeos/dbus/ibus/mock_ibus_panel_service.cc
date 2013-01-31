// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/mock_ibus_panel_service.h"

namespace chromeos {

MockIBusPanelService::MockIBusPanelService() {
}

MockIBusPanelService::~MockIBusPanelService() {
}

void MockIBusPanelService::SetUpCandidateWindowHandler(
    IBusPanelCandidateWindowHandlerInterface* handler) {
}

void MockIBusPanelService::SetUpPropertyHandler(
    IBusPanelPropertyHandlerInterface* handler) {
}

void MockIBusPanelService::CandidateClicked(uint32 index,
                                            ibus::IBusMouseButton button,
                                            uint32 state) {
}

void MockIBusPanelService::CursorUp() {
}

void MockIBusPanelService::CursorDown() {
}

void MockIBusPanelService::PageUp() {
}

void MockIBusPanelService::PageDown() {
}

}  // namespace chromeos
