// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/intents/native_services.h"
#include "webkit/glue/web_intent_service_data.h"

namespace web_intents {

#define NATIVE_SCHEME "chrome-intents-native"

const char kChromeNativeSerivceScheme[] = NATIVE_SCHEME;
const char kNativeFilePickerUrl[] = NATIVE_SCHEME "://file-picker";

NativeServiceRegistry::NativeServiceRegistry() {}

void NativeServiceRegistry::GetSupportedServices(
    const string16& action,
    IntentServiceList* services) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kWebIntentsNativeServicesEnabled))
    return;

#if defined(TOOLKIT_VIEWS)
  if (EqualsASCII(action, web_intents::kActionPick)) {
    // File picker registrations.
    webkit_glue::WebIntentServiceData service(
        ASCIIToUTF16(kActionPick),
        ASCIIToUTF16("*/*"),  // handle any MIME-type
        string16(),
        GURL(web_intents::kNativeFilePickerUrl),
        FilePickerFactory::GetServiceTitle());
    service.disposition = webkit_glue::WebIntentServiceData::DISPOSITION_NATIVE;

    services->push_back(service);
  }
#endif
}

NativeServiceFactory::NativeServiceFactory() {}

IntentServiceHost* NativeServiceFactory::CreateServiceInstance(
    const GURL& service_url,
    const webkit_glue::WebIntentData& intent) {

#if defined(TOOLKIT_VIEWS)
  if (service_url.spec() == kNativeFilePickerUrl) {
    return FilePickerFactory::CreateServiceInstance(intent);
  }
#endif

  return NULL;  // couldn't create instance
}

}  // web_intents namespace
