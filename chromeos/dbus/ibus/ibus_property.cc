// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_property.h"

#include <string>
#include "base/logging.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "chromeos/dbus/ibus/ibus_object.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "dbus/message.h"

namespace chromeos {

bool CHROMEOS_EXPORT PopIBusProperty(dbus::MessageReader* reader,
                                     IBusProperty* property) {
  IBusObjectReader ibus_property_reader("IBusProperty", reader);
  if (!ibus_property_reader.Init())
    return false;

  std::string key;
  if (!ibus_property_reader.PopString(&key)) {
    LOG(ERROR) << "Invalid variant structure[IBusProperty]: "
               << "1st argument should be string.";
    return false;
  }
  property->set_key(key);

  uint32 type = 0;
  if (!ibus_property_reader.PopUint32(&type)) {
    LOG(ERROR) << "Invalid variant structure[IBusProperty]: "
               << "2nd argument should be unsigned 32bit integer.";
    return false;
  }
  if (type > 4UL) {
    LOG(ERROR) << "Invalid value for IBusProperty type";
    return false;
  }
  property->set_type(static_cast<IBusProperty::IBusPropertyType>(type));

  std::string label;
  if (!ibus_property_reader.PopStringFromIBusText(&label)) {
    LOG(ERROR) << "Invalid variant structure[IBusProperty]: "
               << "3rd argument should be IBusText.";
    return false;
  }
  property->set_label(label);

  // The 4th string argument represents icon path, but not supported in
  // Chrome OS.
  std::string icon;
  if (!ibus_property_reader.PopString(&icon)) {
    LOG(ERROR) << "Invalid variant structure[IBusProperty]: "
               << "4th argument should be string.";
  }

  std::string tooltip;
  if (!ibus_property_reader.PopStringFromIBusText(&tooltip)) {
    LOG(ERROR) << "Invalid variant structure[IBusProperty]: "
               << "5th argument should be IBusText.";
    return false;
  }
  property->set_tooltip(tooltip);

  // The 6th bool argument represents whether the property is event sensitive or
  // not, but not supported in Chrome OS.
  bool sensitive = true;
  if (!ibus_property_reader.PopBool(&sensitive)) {
    LOG(ERROR) << "Invalid variant structure[IBusProperty]: "
               << "6th argument should be boolean.";
  }

  bool visible = true;
  if (!ibus_property_reader.PopBool(&visible)) {
    LOG(ERROR) << "Invalid variant structure[IBusProperty]: "
               << "7th argument should be boolean.";
    return false;
  }
  property->set_visible(visible);

  uint32 state = 0;
  if (!ibus_property_reader.PopUint32(&state)) {
    LOG(ERROR) << "Invalid variant structure[IBusProperty]: "
               << "8th argument should be unsigned 32bit integer.";
    return false;
  }

  if (type > 3UL) {
    LOG(ERROR) << "Invalid value for IBusProperty.";
    return false;
  }

  DCHECK_LE(state, 3UL);
  if (state == ibus::IBUS_PROPERTY_STATE_INCONSISTENT) {
    LOG(ERROR) << "PROP_STATE_INCONSISTENT is not supported in Chrome OS.";
  } else {
    property->set_checked(state == ibus::IBUS_PROPERTY_STATE_CHECKED);
  }

  if (!ibus_property_reader.PopIBusPropertyList(
      property->mutable_sub_properties())) {
    LOG(ERROR) << "Invalid variant structure[IBusProperty]: "
               << "9th argument should be IBusPropList.";
    return false;
  }

  return true;
}

bool CHROMEOS_EXPORT PopIBusPropertyList(dbus::MessageReader* reader,
                                         IBusPropertyList* property_list) {
  IBusObjectReader ibus_property_reader("IBusPropList", reader);
  if (!ibus_property_reader.Init())
    return false;

  dbus::MessageReader property_array_reader(NULL);
  if (!ibus_property_reader.PopArray(&property_array_reader)) {
    LOG(ERROR) << "Invalid variant structure[IBusPropList]: "
               << "1st argument should be array.";
    return false;
  }
  property_list->clear();
  while (property_array_reader.HasMoreData()) {
    IBusProperty* property = new IBusProperty;
    if (!PopIBusProperty(&property_array_reader, property)) {
      LOG(ERROR) << "Invalid variant structure[IBusPropList]: "
                 << "1st argument should be array of IBusProperty.";
      return false;
    }
    property_list->push_back(property);
  }

  return true;
}

void CHROMEOS_EXPORT AppendIBusProperty(const IBusProperty& property,
                                        dbus::MessageWriter* writer) {
  IBusObjectWriter ibus_property_writer("IBusProperty", "suvsvbbuv", writer);
  ibus_property_writer.CloseHeader();

  ibus_property_writer.AppendString(property.key());
  ibus_property_writer.AppendUint32(static_cast<uint32>(property.type()));
  ibus_property_writer.AppendStringAsIBusText(property.label());
  ibus_property_writer.AppendString("");  // The icon path is not supported.
  ibus_property_writer.AppendStringAsIBusText(property.tooltip());
  // The event sensitive field is not supported.
  ibus_property_writer.AppendBool(false);
  ibus_property_writer.AppendBool(property.visible());
  ibus_property_writer.AppendUint32(static_cast<uint32>(property.checked()));
  ibus_property_writer.AppendIBusPropertyList(property.sub_properties());
  ibus_property_writer.CloseAll();
}

void CHROMEOS_EXPORT AppendIBusPropertyList(
    const IBusPropertyList& property_list,
    dbus::MessageWriter* writer) {
  IBusObjectWriter ibus_property_list_writer("IBusPropList", "av", writer);
  ibus_property_list_writer.CloseHeader();
  dbus::MessageWriter property_list_writer(NULL);
  ibus_property_list_writer.OpenArray("v", &property_list_writer);
  for (size_t i = 0; i < property_list.size(); ++i) {
    AppendIBusProperty(*(property_list[i]), &property_list_writer);
  }
  ibus_property_list_writer.CloseContainer(&property_list_writer);
  ibus_property_list_writer.CloseAll();
}


///////////////////////////////////////////////////////////////////////////////
// IBusProperty
IBusProperty::IBusProperty()
    : type_(IBUS_PROPERTY_TYPE_NORMAL),
      visible_(false),
      checked_(false) {
}

IBusProperty::~IBusProperty() {
}

}  // namespace chromeos
