// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_ENGINE_SERVICE_H_
#define CHROMEOS_DBUS_IBUS_IBUS_ENGINE_SERVICE_H_

#include <string>
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/ibus/ibus_constants.h"

namespace chromeos {

class IBusText;
class IBusEngineHandlerInterface;

// A class to make the actual DBus method call handling for IBusEngine service.
// The exported method call is used by ibus-demon to process key event, because
// Chrome works engine service if the extension IME is enabled. This class is
// managed by DBusThreadManager.
class CHROMEOS_EXPORT IBusEngineService {
 public:
  // Following value should be same in
  // http://ibus.googlecode.com/svn/docs/ibus-1.4/ibus-ibustypes.html#IBusPreeditFocusMode
  enum IBusEnginePreeditFocusOutMode {
    IBUS_ENGINE_PREEEDIT_FOCUS_OUT_MODE_CLEAR = 0,
    IBUS_ENGINE_PREEEDIT_FOCUS_OUT_MODE_COMMIT = 1,
  };

  virtual ~IBusEngineService();

  // Sets a new IBus engine handler and old handler will be overridden.
  // This class doesn't take the ownership of |handler|.
  virtual void SetEngine(IBusEngineHandlerInterface* handler) = 0;

  // Unsets the IBus engine handler if |handler| equals to current engine
  // handler.
  virtual void UnsetEngine(IBusEngineHandlerInterface* handler) = 0;

  // Emits UpdatePreedit signal.
  virtual void UpdatePreedit(const IBusText& ibus_text,
                             uint32 cursor_pos,
                             bool is_visible,
                             IBusEnginePreeditFocusOutMode mode) = 0;
  // Emits UpdateAuxiliaryText signal.
  virtual void UpdateAuxiliaryText(const IBusText& ibus_text,
                                   bool is_visible) = 0;
  // Emits ForwardKeyEvent signal.
  virtual void ForwardKeyEvent(uint32 keyval, uint32 keycode, uint32 state) = 0;
  // Emits RequireSurroundingText signal.
  virtual void RequireSurroundingText() = 0;
  // Emits DeleteSurroundingText signal.
  virtual void DeleteSurroundingText(int32 offset, uint32 length) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static CHROMEOS_EXPORT IBusEngineService* Create();

 protected:
  // Create() should be used instead.
  IBusEngineService();

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusEngineService);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_ENGINE_SERVICE_H_
