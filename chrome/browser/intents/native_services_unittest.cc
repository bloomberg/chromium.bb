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

TEST(NativeServiceRegistryTest, GetSupportedServices) {
#if !defined(ANDROID)
  // enable native services feature, then check results again
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kWebIntentsNativeServicesEnabled);

  web_intents::IntentServiceList services;
  web_intents::NativeServiceRegistry registry;

  registry.GetSupportedServices(ASCIIToUTF16("dothedew"), &services);

  ASSERT_EQ(0U, services.size());

  registry.GetSupportedServices(
      ASCIIToUTF16(web_intents::kActionPick), &services);

  ASSERT_EQ(1U, services.size());

  // verify the service returned is for "pick"
  EXPECT_EQ(ASCIIToUTF16(web_intents::kActionPick), services[0].action);
  EXPECT_EQ(GURL(web_intents::kNativeFilePickerUrl), services[0].service_url);
#endif
}

TEST(NativeServiceRegistryTest, GetSupportedServicesDisabled) {
#if !defined(ANDROID)
  web_intents::IntentServiceList services;
  web_intents::NativeServiceRegistry registry;

  registry.GetSupportedServices(
      ASCIIToUTF16(web_intents::kActionPick), &services);

  ASSERT_EQ(0U, services.size());
#endif
}

}  // namespace
