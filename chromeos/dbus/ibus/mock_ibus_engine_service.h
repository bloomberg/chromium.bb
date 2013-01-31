// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_MOCK_IBUS_ENGINE_SERVICE_H_
#define CHROMEOS_DBUS_IBUS_MOCK_IBUS_ENGINE_SERVICE_H_

#include <string>
#include "chromeos/dbus/ibus/ibus_engine_service.h"

namespace chromeos {

class MockIBusEngineService : public IBusEngineService {
 public:
  MockIBusEngineService();
  virtual ~MockIBusEngineService();

  // IBusEngineService overrides.
  virtual void SetEngine(IBusEngineHandlerInterface* handler) OVERRIDE;
  virtual void UnsetEngine() OVERRIDE;
  virtual void RegisterProperties(
      const IBusPropertyList& property_list) OVERRIDE;
  virtual void UpdatePreedit(const IBusText& ibus_text,
                             uint32 cursor_pos,
                             bool is_visible,
                             IBusEnginePreeditFocusOutMode mode) OVERRIDE;
  virtual void UpdateAuxiliaryText(const IBusText& ibus_text,
                                   bool is_visible) OVERRIDE;
  virtual void UpdateLookupTable(const IBusLookupTable& lookup_table,
                                 bool is_visible) OVERRIDE;
  virtual void UpdateProperty(const IBusProperty& property) OVERRIDE;
  virtual void ForwardKeyEvent(uint32 keyval, uint32 keycode,
                               uint32 state) OVERRIDE;
  virtual void RequireSurroundingText() OVERRIDE;
  virtual void CommitText(const std::string& text) OVERRIDE;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_MOCK_IBUS_ENGINE_SERVICE_H_
