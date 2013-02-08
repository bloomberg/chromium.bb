// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/native_services.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/url_pattern.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/web_intent_service_data.h"

namespace {

using web_intents::NativeServiceRegistry;

TEST(NativeServiceRegistryTest, GetSupportedServices) {
#if !defined(ANDROID)
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kWebIntentsNativeServicesEnabled);

  NativeServiceRegistry::IntentServiceList services;
  NativeServiceRegistry registry;
  typedef NativeServiceRegistry::IntentServiceList::const_iterator Iter;

  registry.GetSupportedServices(ASCIIToUTF16("dothedew"), &services);

  ASSERT_EQ(0U, services.size());
  for (Iter it = services.begin(); it != services.end(); ++it) {
    ADD_FAILURE() << "Unexpected handler for Dew: " << *it;
  }

  registry.GetSupportedServices(
      ASCIIToUTF16(web_intents::kActionPick), &services);

  EXPECT_EQ(1U, services.size());
  if (services.size() == 1) {
    // Verify the service returned is for "pick".
    EXPECT_EQ(ASCIIToUTF16(web_intents::kActionPick), services[0].action);
    EXPECT_EQ(GURL(web_intents::kNativeFilePickerUrl), services[0].service_url);
  } else {
    for (Iter it = services.begin(); it != services.end(); ++it) {
      ADD_FAILURE() << "Too many services for pick action: " << *it;
    }
  }
#endif
}

TEST(NativeServiceRegistryTest, GetSupportedServicesDisabled) {
#if !defined(ANDROID)
  NativeServiceRegistry::IntentServiceList services;
  NativeServiceRegistry registry;

  registry.GetSupportedServices(
      ASCIIToUTF16(web_intents::kActionPick), &services);

  ASSERT_EQ(0U, services.size());
#endif
}

}  // namespace
