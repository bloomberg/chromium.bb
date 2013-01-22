// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_PANEL_SERVICE_H_
#define CHROMEOS_DBUS_IBUS_IBUS_PANEL_SERVICE_H_

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

// TODO(nona): Remove ibus namespace after complete libibus removal.
namespace ibus {
class IBusLookupTable;
class IBusProperty;
class IBusText;
typedef ScopedVector<IBusProperty> IBusPropertyList;

// A interface to handle the candidate window related method call.
class CHROMEOS_EXPORT IBusPanelCandidateWindowHandlerInterface {
 public:
  virtual ~IBusPanelCandidateWindowHandlerInterface() {}

  // Called when the IME updates the lookup table.
  virtual void UpdateLookupTable(const ibus::IBusLookupTable& table,
                                 bool visible) = 0;

  // Called when the IME hides the lookup table.
  virtual void HideLookupTable() = 0;

  // Called when the IME updates the auxiliary text. The |text| is given in
  // UTF-8 encoding.
  virtual void UpdateAuxiliaryText(const std::string& text, bool visible) = 0;

  // Called when the IME hides the auxiliary text.
  virtual void HideAuxiliaryText() = 0;

  // Called when the IME updates the preedit text. The |text| is given in
  // UTF-8 encoding.
  virtual void UpdatePreeditText(const std::string& text, uint32 cursor_pos,
                                 bool visible) = 0;

  // Called when the IME hides the preedit text.
  virtual void HidePreeditText() = 0;

 protected:
  IBusPanelCandidateWindowHandlerInterface() {}
};

// A interface to handle the property related method call.
class CHROMEOS_EXPORT IBusPanelPropertyHandlerInterface {
 public:
  virtual ~IBusPanelPropertyHandlerInterface() {}

  // Called when a new property is registered.
  virtual void RegisterProperties(const ibus::IBusPropertyList& properties) = 0;

  // Called when current property is updated.
  virtual void UpdateProperty(const ibus::IBusProperty& property) = 0;

 protected:
  IBusPanelPropertyHandlerInterface() {}
};

// A class to make the actual DBus method call handling for IBusPanel service.
// The exported method call is used by ibus-daemon to process candidate window
// related event, because Chrome works as candidate window. This class is
// managed by DBusThreadManager.
class CHROMEOS_EXPORT IBusPanelService {
 public:
  virtual ~IBusPanelService();

  // Sets up candidate window panel service with |handler|. This function can be
  // called multiple times and also can be passed |handler| as NULL. Caller must
  // release |handler|.
  virtual void SetUpCandidateWindowHandler(
      IBusPanelCandidateWindowHandlerInterface* handler) = 0;

  // Sets up property panel service with |handler|. This function can be called
  // multiple times and also can be passed |handler| as NULL. Caller must
  // release |handler|.
  virtual void SetUpPropertyHandler(
      IBusPanelPropertyHandlerInterface* handler) = 0;

  // Emits CandidateClicked signal.
  virtual void CandidateClicked(uint32 index,
                                ibus::IBusMouseButton button,
                                uint32 state) = 0;

  // Emits CursorUp signal.
  virtual void CursorUp() = 0;

  // Emits CursorDown signal.
  virtual void CursorDown() = 0;

  // Emits PageUp signal.
  virtual void PageUp() = 0;

  // Emits PageDown signal.
  virtual void PageDown() = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static CHROMEOS_EXPORT IBusPanelService* Create(
      DBusClientImplementationType type,
      dbus::Bus* bus);

 protected:
  // Create() should be used instead.
  IBusPanelService();

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusPanelService);
};

}  // namespace ibus
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_PANEL_SERVICE_H_
