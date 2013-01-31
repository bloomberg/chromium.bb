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

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace chromeos {

class IBusLookupTable;
class IBusProperty;
class IBusText;
typedef ScopedVector<IBusProperty> IBusPropertyList;

// A interface to handle the engine handler method call.
class CHROMEOS_EXPORT IBusEngineHandlerInterface {
 public:
  typedef base::Callback<void (bool consumed)> KeyEventDoneCallback;

  // Following capability mask is introduced from
  // http://ibus.googlecode.com/svn/docs/ibus-1.4/ibus-ibustypes.html#IBusCapabilite
  // TODO(nona): Move to ibus_contants and merge one in ui/base/ime/*
  enum IBusCapability {
    IBUS_CAPABILITY_PREEDIT_TEXT = 1U,
    IBUS_CAPABILITY_FOCUS = 8U,
  };

  virtual ~IBusEngineHandlerInterface() {}

  // Called when the Chrome input field get the focus.
  virtual void FocusIn() = 0;

  // Called when the Chrome input field lose the focus.
  virtual void FocusOut() = 0;

  // Called when the IME is enabled.
  virtual void Enable() = 0;

  // Called when the IME is disabled.
  virtual void Disable() = 0;

  // Called when a property is activated or changed.
  virtual void PropertyActivate(const std::string& property_name,
                                ibus::IBusPropertyState property_state) = 0;

  // Called when a property is shown.
  virtual void PropertyShow(const std::string& property_name) = 0;

  // Called when a property is hidden.
  virtual void PropertyHide(const std::string& property_name) = 0;

  // Called when the Chrome input field set their capabilities.
  virtual void SetCapability(IBusCapability capability) = 0;

  // Called when the IME is reset.
  virtual void Reset() = 0;

  // Called when the key event is received. The |keycode| is raw layout
  // independent keycode. The |keysym| is result of XLookupString function
  // which translate |keycode| to keyboard layout dependent symbol value.
  // Actual implementation must call |callback| after key event handling.
  // For example: key press event for 'd' key on us layout and dvorak layout.
  //                  keyval keycode state
  //      us layout :  0x64   0x20    0x00
  //  dvorak layout :  0x65   0x20    0x00
  virtual void ProcessKeyEvent(uint32 keysym, uint32 keycode, uint32 state,
                               const KeyEventDoneCallback& callback) = 0;

  // Called when the candidate in lookup table is clicked. The |index| is 0
  // based candidate index in lookup table. The |state| is same value as
  // GdkModifierType in
  // http://developer.gnome.org/gdk/stable/gdk-Windows.html#GdkModifierType
  virtual void CandidateClicked(uint32 index, ibus::IBusMouseButton button,
                                uint32 state) = 0;

  // Called when a new surrounding text is set. The |text| is surrounding text
  // and |cursor_pos| is 0 based index of cursor position in |text|. If there is
  // selection range, |anchor_pos| represents opposite index from |cursor_pos|.
  // Otherwise |anchor_pos| is equal to |cursor_pos|.
  virtual void SetSurroundingText(const std::string& text, uint32 cursor_pos,
                                  uint32 anchor_pos) = 0;

 protected:
  IBusEngineHandlerInterface() {}
};

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

  // Unsets the current IBus engine handler.
  virtual void UnsetEngine() = 0;

  // Emits RegisterProperties signal.
  virtual void RegisterProperties(
      const IBusPropertyList& property_list) = 0;
  // Emits UpdatePreedit signal.
  virtual void UpdatePreedit(const IBusText& ibus_text,
                             uint32 cursor_pos,
                             bool is_visible,
                             IBusEnginePreeditFocusOutMode mode) = 0;
  // Emits UpdateAuxiliaryText signal.
  virtual void UpdateAuxiliaryText(const IBusText& ibus_text,
                                   bool is_visible) = 0;
  // Emits UpdateLookupTable signal.
  virtual void UpdateLookupTable(const IBusLookupTable& lookup_table,
                                 bool is_visible) = 0;
  // Emits UpdateProperty signal.
  virtual void UpdateProperty(const IBusProperty& property) = 0;
  // Emits ForwardKeyEvent signal.
  virtual void ForwardKeyEvent(uint32 keyval, uint32 keycode, uint32 state) = 0;
  // Emits RequireSurroundingText signal.
  virtual void RequireSurroundingText() = 0;
  // Emits CommitText signal.
  virtual void CommitText(const std::string& text) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static CHROMEOS_EXPORT IBusEngineService* Create(
      DBusClientImplementationType type,
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path);

 protected:
  // Create() should be used instead.
  IBusEngineService();

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusEngineService);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_ENGINE_SERVICE_H_
