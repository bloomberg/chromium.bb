// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth_low_energy/utils.h"

namespace extensions {
namespace api {
namespace bluetooth_low_energy {

scoped_ptr<base::DictionaryValue> CharacteristicToValue(Characteristic* from) {
  // Copy the properties. Use Characteristic::ToValue to generate the result
  // dictionary without the properties, to prevent json_schema_compiler from
  // failing.
  std::vector<CharacteristicProperty> properties = from->properties;
  from->properties.clear();
  scoped_ptr<base::DictionaryValue> to = from->ToValue();

  // Manually set each property.
  scoped_ptr<base::ListValue> property_list(new base::ListValue());
  for (std::vector<CharacteristicProperty>::iterator iter = properties.begin();
       iter != properties.end();
       ++iter)
    property_list->Append(new base::StringValue(ToString(*iter)));

  to->Set("properties", property_list.release());

  return to.Pass();
}

}  // namespace bluetooth_low_energy
}  // namespace api
}  // namespace extensions
