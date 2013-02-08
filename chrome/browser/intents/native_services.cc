// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Native services are implemented with UI code necessitating portions
// of native_services.h to be defined in
// chrome/browser/ui/intents/native_file_picker_service.cc

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/native_services.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/common/chrome_switches.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/web_intent_service_data.h"

namespace web_intents {

const char kNativeFilePickerUrl[] = "chrome-intents-native://file-picker";

NativeServiceRegistry::NativeServiceRegistry() {}
NativeServiceRegistry::~NativeServiceRegistry() {}

void NativeServiceRegistry::GetSupportedServices(
    const string16& action,
    IntentServiceList* services) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kWebIntentsNativeServicesEnabled))
    return;

#if !defined(ANDROID)
  if (EqualsASCII(action, kActionPick)) {
    // File picker registrations.
    webkit_glue::WebIntentServiceData service(
        ASCIIToUTF16(kActionPick),
        ASCIIToUTF16("*/*"),  // Handle any MIME-type.
        // This is an action/type based service, so we supply an empty scheme.
        string16(),
        GURL(kNativeFilePickerUrl),
        FilePickerFactory::GetServiceTitle());
    service.disposition = webkit_glue::WebIntentServiceData::DISPOSITION_NATIVE;

    services->push_back(service);
  }
#endif
}

NativeServiceFactory::NativeServiceFactory() {}
NativeServiceFactory::~NativeServiceFactory() {}

IntentServiceHost* NativeServiceFactory::CreateServiceInstance(
    const GURL& service_url,
    const webkit_glue::WebIntentData& intent,
    content::WebContents* web_contents) {

#if !defined(ANDROID)
  if (service_url.spec() == kNativeFilePickerUrl) {
    return FilePickerFactory::CreateServiceInstance(intent, web_contents);
  }
#endif

  return NULL;  // Couldn't create an instance.
}

}  // namespace web_intents
