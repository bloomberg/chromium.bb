// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_INPUT_CONTEXT_CLIENT_H_
#define CHROMEOS_DBUS_IBUS_IBUS_INPUT_CONTEXT_CLIENT_H_

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// TODO(nona): Remove ibus namespace after complete libibus removal.
namespace ibus {
class IBusText;
}  // namespace

class CHROMEOS_EXPORT IBusInputContextHandlerInterface {
 public:
  // Called when the engine commit a text.
  virtual void CommitText(const ibus::IBusText& text) = 0;

  // Called when the engine forward a key event.
  virtual void ForwardKeyEvent(uint32 keyval, uint32 keycode, uint32 state) = 0;

  // Called when the engine update preedit stroing.
  virtual void UpdatePreeditText(const ibus::IBusText& text,
                                 uint32 cursor_pos,
                                 bool visible) = 0;

  // Called when the engine request showing preedit string.
  virtual void ShowPreeditText() = 0;

  // Called when the engine request hiding preedit string.
  virtual void HidePreeditText() = 0;
};

// A class to make the actual DBus calls for IBusInputContext service.
// The ibus-daemon creates object paths on demand, so the target object path is
// not determined before calling CreateInputContext. It is good to initialize
// this class at the callback from CreateInputContext in IBusClient. This class
// is managed by DBusThreadManager as singleton instance, so we can handle only
// one input context but it is enough for ChromeOS.
class CHROMEOS_EXPORT IBusInputContextClient {
 public:
  typedef base::Callback<void(const ibus::Rect& cursor_location,
                              const ibus::Rect& composition_head)>
      SetCursorLocationHandler;
  typedef base::Callback<void(bool is_keyevent_used)> ProcessKeyEventCallback;
  typedef base::Callback<void()> ErrorCallback;

  virtual ~IBusInputContextClient();

  // Creates object proxy and connects signals.
  virtual void Initialize(dbus::Bus* bus,
                          const dbus::ObjectPath& object_path) = 0;

  // Sets input context handler. This function can be called multiple times and
  // also can be passed |handler| as NULL. Caller must release |handler|.
  virtual void SetInputContextHandler(
      IBusInputContextHandlerInterface* handler) = 0;

  // Sets SetCursorLocation handler.
  virtual void SetSetCursorLocationHandler(
      const SetCursorLocationHandler& set_cursor_location_handler) = 0;

  // Unset SetCursorLocation handler.
  virtual void UnsetSetCursorLocationHandler() = 0;

  // Resets object proxy. If you want to use InputContext again, should call
  // Initialize function again.
  virtual void ResetObjectProxy() = 0;

  // Returns true if the object proxy is ready to communicate with ibus-daemon,
  // otherwise return false.
  virtual bool IsObjectProxyReady() const = 0;

  // Invokes SetCapabilities method call.
  virtual void SetCapabilities(uint32 capability) = 0;
  // Invokes FocusIn method call.
  virtual void FocusIn() = 0;
  // Invokes FocusOut method call.
  virtual void FocusOut() = 0;
  // Invokes Reset method call.
  virtual void Reset() = 0;
  // Invokes SetCursorLocation method call.
  virtual void SetCursorLocation(const ibus::Rect& cursor_location,
                                 const ibus::Rect& composition_head) = 0;
  // Invokes ProcessKeyEvent method call. |callback| should not be null.
  virtual void ProcessKeyEvent(uint32 keyval,
                               uint32 keycode,
                               uint32 state,
                               const ProcessKeyEventCallback& callback,
                               const ErrorCallback& error_callback) = 0;

  // Invokes SetSurroundingText method call. |start_index| is inclusive and
  // |end_index| is exclusive.
  virtual void SetSurroundingText(const std::string& text,
                                  uint32 start_index,
                                  uint32 end_index) = 0;

  // Invokes PropertyActivate method call. The PROP_STATE_INCONSISTENT in
  // original IBus spec is not supported in Chrome.
  virtual void PropertyActivate(const std::string& key,
                                ibus::IBusPropertyState state) = 0;

  // Returns true if the current input method is XKB layout.
  virtual bool IsXKBLayout() = 0;

  // Sets current input method is XKB layout or not.
  virtual void SetIsXKBLayout(bool is_xkb_layout) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static CHROMEOS_EXPORT IBusInputContextClient* Create(
      DBusClientImplementationType type);

 protected:
  // Create() should be used instead.
  IBusInputContextClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusInputContextClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_INPUT_CONTEXT_CLIENT_H_
