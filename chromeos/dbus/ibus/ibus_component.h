// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_COMPONENT_H_
#define CHROMEOS_DBUS_IBUS_IBUS_COMPONENT_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"

namespace dbus {
class MessageWriter;
class MessageReader;
}  // namespace dbus

namespace chromeos {

// The IBusComponents is one of IBusObjects and it contains one or more
// IBusEngineDesc object which describes details of engine information. This
// object is in used an engine's initialization.
//
// DATA STRUCTURE OVERVIEW:
//
// IBusEngineDesc: (signature is "ssssssssusss")
// variant  struct {
//     string "IBusEngineDesc"
//     array []
//     string "_ext_ime"  // The engine name.
//     string "Mock IME"  // The long engine name.
//     string "Mock IME"  // The engine description.
//     string "en"  // The language identifier.
//     string ""  // The license name field.(not in use).
//     string "MockExtensionIME"  // The author name.
//     string ""  // The icon path (not in use).
//     string "us"  // The keyboard layout.
//     uint32 0  // The engine rank. (not in use).
//     string ""  // The hotkey to switch IME.(not in use)
//     string ""  // The symbol character of this engine (not in use).
//     string ""  // The command line to execute this engine (not in use).
// }
//
// IBusComponent: (signature is "ssssssssavav")
// variant       struct {
//     string "IBusComponent"
//     array []
//     string "org.freedesktop.IBus._extension_ime"  // The component name.
//     string "Mock IME with Extension API" // The component description.
//     string ""  // The version of component.(not in use)
//     string ""  // The license name field. (not in use).
//     string "MockExtensionIME" // The author name.
//     string ""  // The URL to home page(not in use).
//     string ""  // The executable path to component (not in use).
//     string ""  // The text domain field(not in use).
//     array []  // The observed path object array(not in use).
//     array [
//         variant  struct {
//             string "IBusEngineDesc"
//             array []
//             string "_ext_ime"
//             string "Mock IME with Extension API"
//             string "Mock IME with Extension API"
//             string "en"
//             string ""
//             string "MockExtensionIME"
//             string ""
//             string "us"
//             uint32 0
//             string ""
//             string ""
//             string ""
//         }
//     ]
// }
class IBusComponent;

// Pops a IBusComponent from |reader|.
// Returns false if an error occurs.
bool CHROMEOS_EXPORT PopIBusComponent(dbus::MessageReader* reader,
                                      IBusComponent* ibus_component);

// Appends a IBusComponent to |writer|.
void CHROMEOS_EXPORT AppendIBusComponent(const IBusComponent& ibus_component,
                                         dbus::MessageWriter* writer);

// Handles IBusComponent object which is used in dbus communication with
// ibus-daemon. The IBusComponent is one of IBusObjects and it contains array of
// IBusEngineDesc. The IBusEngineDesc is another IBusObject, but it is
// represented as member structure of IBusComponent because it is only used in
// IBusComponent.
class CHROMEOS_EXPORT IBusComponent {
 public:
  struct EngineDescription {
    EngineDescription();
    EngineDescription(const std::string& engine_id,
                      const std::string& display_name,
                      const std::string& description,
                      const std::string& language_code,
                      const std::string& author,
                      const std::string& layout);
    ~EngineDescription();
    std::string engine_id;  // The engine id.
    std::string display_name;  // The display name.
    std::string description;  // The engine description.
    std::string language_code;  // The engine's language(ex. "en").
    std::string author;  // The author of engine.
    std::string layout;  // The keyboard layout of engine.
  };

  IBusComponent();
  virtual ~IBusComponent();

  // The component name (ex. "org.freedesktop.IBus.Mozc").
  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name;}

  // The component description.
  const std::string& description() const { return description_; }
  void set_description(const std::string& description) {
    description_ = description;
  }

  // The component author (ex. "Google Inc.").
  const std::string& author() const { return author_; }
  void set_author(const std::string& author) { author_ = author; }

  // The array of engine description data.
  const std::vector<EngineDescription>& engine_description() const {
    return engine_description_;
  }
  std::vector<EngineDescription>* mutable_engine_description() {
    return &engine_description_;
  }

 private:
  std::string name_;
  std::string description_;
  std::string author_;

  std::vector<EngineDescription> engine_description_;

  DISALLOW_COPY_AND_ASSIGN(IBusComponent);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_COMPONENT_H_
