// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MPRIS_MPRIS_SERVICE_H_
#define COMPONENTS_MPRIS_MPRIS_SERVICE_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"

namespace base {
class Value;
}  // namespace base

namespace dbus {
class MessageWriter;
class MethodCall;
}  // namespace dbus

namespace mpris {

class MprisServiceObserver;

COMPONENT_EXPORT(MPRIS) extern const char kMprisAPIServiceNamePrefix[];
COMPONENT_EXPORT(MPRIS) extern const char kMprisAPIObjectPath[];
COMPONENT_EXPORT(MPRIS) extern const char kMprisAPIInterfaceName[];
COMPONENT_EXPORT(MPRIS) extern const char kMprisAPIPlayerInterfaceName[];

// A D-Bus service conforming to the MPRIS spec:
// https://specifications.freedesktop.org/mpris-spec/latest/
class COMPONENT_EXPORT(MPRIS) MprisService {
 public:
  MprisService();
  ~MprisService();

  static MprisService* GetInstance();

  // Starts the DBus service.
  void StartService();

  void AddObserver(MprisServiceObserver* observer);
  void RemoveObserver(MprisServiceObserver* observer);

  // Setters for properties.
  void SetCanGoNext(bool value);
  void SetCanGoPrevious(bool value);
  void SetCanPlay(bool value);
  void SetCanPause(bool value);

  // Returns the generated service name.
  std::string GetServiceName() const;

  // Used for testing with a mock DBus Bus.
  void SetBusForTesting(scoped_refptr<dbus::Bus> bus) { bus_ = bus; }

 private:
  // MprisService can only have one instance. This holds a pointer to that
  // instance.
  static MprisService* instance_;

  void InitializeProperties();
  void InitializeDbusInterface();
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);
  void OnOwnership(const std::string& service_name, bool success);

  // org.mpris.MediaPlayer2.Player interface.
  void Next(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender response_sender);
  void Previous(dbus::MethodCall* method_call,
                dbus::ExportedObject::ResponseSender response_sender);
  void Pause(dbus::MethodCall* method_call,
             dbus::ExportedObject::ResponseSender response_sender);
  void PlayPause(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender);
  void Stop(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender response_sender);
  void Play(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender response_sender);

  // Used for API methods we don't support.
  void DoNothing(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender);

  // org.freedesktop.DBus.Properties interface.
  void GetAllProperties(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender);
  void GetProperty(dbus::MethodCall* method_call,
                   dbus::ExportedObject::ResponseSender response_sender);

  using PropertyMap = base::flat_map<std::string, base::Value>;

  // Sets a value on the given PropertyMap and sends a PropertiesChanged signal
  // if necessary.
  void SetPropertyInternal(PropertyMap& property_map,
                           const std::string& property_name,
                           const base::Value& new_value);

  // Emits a org.freedesktop.DBus.Properties.PropertiesChanged signal for the
  // given map of changed properties.
  void EmitPropertiesChangedSignal(const PropertyMap& changed_properties);

  // Writes all properties onto writer.
  void AddPropertiesToWriter(dbus::MessageWriter* writer,
                             const PropertyMap& properties);

  // Map of org.mpris.MediaPlayer2 interface properties.
  PropertyMap media_player2_properties_;

  // Map of org.mpris.MediaPlayer2.Player interface properties.
  PropertyMap media_player2_player_properties_;

  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* exported_object_;

  // The generated service name given to |bus_| when requesting ownership.
  const std::string service_name_;

  // The number of methods that have been successfully exported through
  // |exported_object_|.
  int num_methods_exported_ = 0;

  // True if we have finished creating the DBus service and received ownership.
  bool service_ready_ = false;

  base::ObserverList<MprisServiceObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MprisService);
};

}  // namespace mpris

#endif  // COMPONENTS_MPRIS_MPRIS_SERVICE_H_
