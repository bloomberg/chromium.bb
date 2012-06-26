// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_INPUT_CONTEXT_CLIENT_H_
#define CHROMEOS_DBUS_IBUS_IBUS_INPUT_CONTEXT_CLIENT_H_
#pragma once

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// TODO(nona): Remove ibus namespace after complete libibus removal.
namespace ibus {
class IBusText;
}  // namespace

// A class to make the actual DBus calls for IBusInputContext service.
// The ibus-daemon creates object paths on demand, so the target object path is
// not determined before calling CreateInputContext. It is good to initialize
// this class at the callback from CreateInputContext in IBusClient. This class
// is managed by DBusThreadManager as singleton instance, so we can handle only
// one input context but it is enough for ChromeOS.
class CHROMEOS_EXPORT IBusInputContextClient {
 public:
  typedef base::Callback<void(const ibus::IBusText& text)> CommitTextHandler;
  typedef base::Callback<void(uint32 keyval, uint32 keycode, uint32 state)>
      ForwardKeyEventHandler;
  typedef base::Callback<void(const ibus::IBusText& text,
                              uint32 cursor_pos,
                              bool visible)>
      UpdatePreeditTextHandler;
  typedef base::Callback<void()> ShowPreeditTextHandler;
  typedef base::Callback<void()> HidePreeditTextHandler;
  typedef base::Callback<void(bool is_keyevent_used)> ProcessKeyEventCallback;

  virtual ~IBusInputContextClient();

  // Creates object proxy and connects signals.
  virtual void Initialize(dbus::Bus* bus,
                          const dbus::ObjectPath& object_path) = 0;

  // Resets object proxy. If you want to use InputContext again, should call
  // Initialize function again.
  virtual void ResetObjectProxy() = 0;

  // Returns true if the object proxy is ready to communicate with ibus-daemon,
  // otherwise return false.
  virtual bool IsObjectProxyReady() const = 0;

  // Signal handler accessors. Setting function can be called multiple times. If
  // you call setting function multiple times, previous callback will be
  // overwritten.
  // Sets CommitText signal handler.
  virtual void SetCommitTextHandler(
      const CommitTextHandler& commit_text_handler) = 0;
  // Sets ForwardKeyEvent signal handler.
  virtual void SetForwardKeyEventHandler(
      const ForwardKeyEventHandler& forward_key_event_handler) = 0;
  // Sets UpdatePreeditText signal handler.
  virtual void SetUpdatePreeditTextHandler(
      const UpdatePreeditTextHandler& update_preedit_text_handler) = 0;
  // Sets ShowPreeditText signal handler.
  virtual void SetShowPreeditTextHandler(
      const ShowPreeditTextHandler& show_preedit_text_handler) = 0;
  // Sets HidePreeditText signal handler.
  virtual void SetHidePreeditTextHandler(
      const HidePreeditTextHandler& hide_preedit_text_handler) = 0;
  // Unsets CommitText signal handler.
  virtual void UnsetCommitTextHandler() = 0;
  // Unsets FowardKeyEvent signal handler.
  virtual void UnsetForwardKeyEventHandler() = 0;
  // Unsets UpdatePreeditText signal handler.
  virtual void UnsetUpdatePreeditTextHandler() = 0;
  // Unsets ShowPreeditText signal handler.
  virtual void UnsetShowPreeditTextHandler() = 0;
  // Unsets HidePreeditText signal handler.
  virtual void UnsetHidePreeditTextHandler() = 0;

  // Invokes SetCapabilities method call.
  virtual void SetCapabilities(uint32 capability) = 0;
  // Invokes FocusIn method call.
  virtual void FocusIn() = 0;
  // Invokes FocusOut method call.
  virtual void FocusOut() = 0;
  // Invokes Reset method call.
  virtual void Reset() = 0;
  // Invokes SetCursorLocation method call.
  virtual void SetCursorLocation(int32 x, int32 y, int32 width,
                                 int32 height) = 0;
  // Invokes ProcessKeyEvent method call. |callback| shold not be null-callback.
  virtual void ProcessKeyEvent(uint32 keyval,
                               uint32 keycode,
                               uint32 state,
                               const ProcessKeyEventCallback& callback) = 0;

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
