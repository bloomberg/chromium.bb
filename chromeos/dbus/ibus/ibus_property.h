// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_PROPERTY_H_
#define CHROMEOS_DBUS_IBUS_IBUS_PROPERTY_H_

#include <string>
#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "chromeos/chromeos_export.h"

namespace dbus {
class MessageWriter;
class MessageReader;
}  // namespace dbus

namespace chromeos {

class IBusProperty;
typedef ScopedVector<IBusProperty> IBusPropertyList;

// Pops a IBusProperty from |reader|.
// Returns false if an error occurs.
bool CHROMEOS_EXPORT PopIBusProperty(dbus::MessageReader* reader,
                                     IBusProperty* property);
// Pops a IBusPropertyList from |reader|.
// Returns false if an error occurs.
bool CHROMEOS_EXPORT PopIBusPropertyList(dbus::MessageReader* reader,
                                         IBusPropertyList* property_list);

// Appends a IBusProperty to |writer|.
void CHROMEOS_EXPORT AppendIBusProperty(const IBusProperty& property,
                                        dbus::MessageWriter* writer);
// Appends a IBusPropertyList to |writer|.
void CHROMEOS_EXPORT AppendIBusPropertyList(
    const IBusPropertyList& property_list,
    dbus::MessageWriter* writer);

// The IBusPropList is one of IBusObjects and it contains array of IBusProperty.
// The IBusProperty contains IBusTexts but only plain string is used in Chrome.
// We treat IBusPropList as scoped_vector of IBusProperty.
//
// DATA STRUCTURE OVERVIEW:
// variant struct {
//   string "IBusProperty"
//   array []
//   string "CompositionMode"  // Key
//   uint32 3  // Type
//   variant struct { // Label
//     string "IBusText"
//     array []
//     string ""
//     variant struct {
//       string "IBusAttrList"
//       array []
//       array []
//     }
//   }
//   string "/usr/share/ibus-mozc/hiragana.png"  // icon
//   variant struct {  // Tooltip
//     string "IBusText"
//     array []
//     string ""
//     variant struct {
//       string "IBusAttrList"
//       array []
//       array []
//     }
//   }
//   boolean true  // sensitive
//   boolean true  // visible
//   uint32 0  // state
//   variant struct {  // sub properties
//     string "IBusPropList"
//     array []
//     array [
//       ... More IBusPropertys
//     ]
//   }
// }
class CHROMEOS_EXPORT IBusProperty {
 public:
  enum IBusPropertyType {
    IBUS_PROPERTY_TYPE_NORMAL = 0,
    IBUS_PROPERTY_TYPE_TOGGLE,
    IBUS_PROPERTY_TYPE_RADIO,
    IBUS_PROPERTY_TYPE_MENU,
    IBUS_PROPERTY_TYPE_SEPARATOR,
  };
  IBusProperty();
  virtual ~IBusProperty();

  // The identity for the IBusProperty.
  std::string key() const { return key_; }
  void set_key(const std::string& key) { key_ = key; }

  // The type of property:
  IBusPropertyType type() const { return type_; }
  void set_type(IBusPropertyType type) { type_ = type; }

  // The string to be shown in UI.
  std::string label() const { return label_; }
  void set_label(const std::string& label) { label_ = label; }

  // The string to be shown in UI as tooltip.
  std::string tooltip() const { return tooltip_; }
  void set_tooltip(const std::string& tooltip) { tooltip_ = tooltip; }

  // True if the property is visible.
  bool visible() const { return visible_; }
  void set_visible(bool visible) { visible_ = visible; }

  // True if the property is checked.
  bool checked() const { return checked_; }
  void set_checked(bool checked) { checked_ = checked; }

  const IBusPropertyList& sub_properties() const { return sub_properties_; }
  IBusPropertyList* mutable_sub_properties() { return &sub_properties_; }

 private:
  std::string key_;
  IBusPropertyType type_;
  std::string label_;
  std::string tooltip_;
  bool visible_;
  bool checked_;
  IBusPropertyList sub_properties_;

  DISALLOW_COPY_AND_ASSIGN(IBusProperty);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_PROPERTY_H_
