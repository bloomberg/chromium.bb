// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/ibus/ibus_property.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "chromeos/dbus/ibus/ibus_object.h"
#include "dbus/message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

const char kSampleKey[] = "Key";
const IBusProperty::IBusPropertyType kSampleType =
    IBusProperty::IBUS_PROPERTY_TYPE_RADIO;
const char kSampleLabel[] = "Label";
const char kSampleTooltip[] = "Tooltip";
const bool kSampleVisible = true;
const bool kSampleChecked = false;

// Sets testing data to |property| with |prefix|.
// This function clears IBusProperty::sub_properties_ and does not add any
// entries into it. The testing data can be checked with CheckProperty function
// with same |prefix|.
void SetProperty(const std::string& prefix, IBusProperty* property) {
  property->set_key(prefix + kSampleKey);
  property->set_type(kSampleType);
  property->set_label(prefix + kSampleLabel);
  property->set_tooltip(prefix + kSampleTooltip);
  property->set_visible(kSampleVisible);
  property->set_checked(kSampleChecked);
  property->mutable_sub_properties()->clear();
}

// Checks testing data in |property| with |prefix|.
// This function does not check IBusProperty::sub_properties_.
bool CheckProperty(const std::string& prefix, const IBusProperty& property) {
  if ((prefix + kSampleKey) != property.key()) {
    LOG(ERROR) << "Does not match IBusProperty::key value: " << std::endl
               << "Expected: " << (prefix + kSampleKey) << std::endl
               << "Actual: " << property.key();
    return false;
  }
  if (kSampleType != property.type()) {
    LOG(ERROR) << "Does not match IBusProperty::type value: " << std::endl
               << "Expected: " << kSampleType << std::endl
               << "Actual: " << property.type();
    return false;
  }
  if ((prefix + kSampleLabel) != property.label()) {
    LOG(ERROR) << "Does not match IBusProperty::label value: " << std::endl
               << "Expected: " << (prefix + kSampleLabel) << std::endl
               << "Actual: " << property.label();
    return false;
  }
  if ((prefix + kSampleTooltip) != property.tooltip()) {
    LOG(ERROR) << "Does not match IBusProperty::tooltip value: " << std::endl
               << "Expected: " << (prefix + kSampleTooltip) << std::endl
               << "Actual: " << property.tooltip();
    return false;
  }
  if (kSampleVisible != property.visible()) {
    LOG(ERROR) << "Does not match IBusProperty::visible value: " << std::endl
               << "Expected: " << kSampleVisible << std::endl
               << "Actual: " << property.visible();
    return false;
  }
  if (kSampleChecked != property.checked()) {
    LOG(ERROR) << "Does not match IBusProperty::state value: " << std::endl
               << "Expected: " << kSampleChecked << std::endl
               << "Actual: " << property.checked();
    return false;
  }
  return true;
}

}  // namespace

TEST(IBusPropertyListTest, WriteReadIBusPropertyTest) {
  const size_t kSubPropertyCount = 16;

  // Create a IBusProperty.
  IBusProperty property;
  SetProperty("Root_", &property);
  for (size_t i = 0; i < kSubPropertyCount; ++i) {
    const std::string prefix = "Sub" + base::Uint64ToString(i);
    IBusProperty* sub_property = new IBusProperty;
    SetProperty(prefix, sub_property);
    property.mutable_sub_properties()->push_back(sub_property);
  }

  // Write a IBusProperty.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  AppendIBusProperty(property, &writer);

  // Read a IBusProperty.
  IBusProperty target_property;
  dbus::MessageReader reader(response.get());
  PopIBusProperty(&reader, &target_property);

  // Check a result.
  EXPECT_TRUE(CheckProperty("Root_", target_property));
  const IBusPropertyList& sub_properties = target_property.sub_properties();
  ASSERT_EQ(kSubPropertyCount, sub_properties.size());
  for (size_t i = 0; i < kSubPropertyCount; ++i) {
    const std::string prefix = "Sub" + base::Uint64ToString(i);
    EXPECT_TRUE(CheckProperty(prefix, *(sub_properties[i])));
  }
}

}  // namespace chromeos
