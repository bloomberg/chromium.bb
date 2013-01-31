// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_MOCK_IBUS_PANEL_SERVICE_H_
#define CHROMEOS_DBUS_IBUS_MOCK_IBUS_PANEL_SERVICE_H_

#include "chromeos/dbus/ibus/ibus_panel_service.h"

namespace chromeos {

class MockIBusPanelService : public IBusPanelService {
 public:
  MockIBusPanelService();
  virtual ~MockIBusPanelService();

  // IBusPanelService overrides.
  virtual void SetUpCandidateWindowHandler(
      IBusPanelCandidateWindowHandlerInterface* handler) OVERRIDE;
  virtual void SetUpPropertyHandler(
      IBusPanelPropertyHandlerInterface* handler) OVERRIDE;
  virtual void CandidateClicked(uint32 index,
                                ibus::IBusMouseButton button,
                                uint32 state) OVERRIDE;
  virtual void CursorUp() OVERRIDE;
  virtual void CursorDown() OVERRIDE;
  virtual void PageUp() OVERRIDE;
  virtual void PageDown() OVERRIDE;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_MOCK_IBUS_PANEL_SERVICE_H_
