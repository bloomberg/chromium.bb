// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_NATIVE_SERVICES_H_
#define CHROME_BROWSER_INTENTS_NATIVE_SERVICES_H_

#include <string>
#include <vector>
#include "base/string16.h"

class GURL;

namespace content {
class WebContents;
class WebIntentsDispatcher;
}

namespace webkit_glue {
 struct WebIntentData;
 struct WebIntentServiceData;
}

namespace web_intents {

class IntentServiceHost;

extern const char kChromeNativeSerivceScheme[];
extern const char kNativeFilePickerUrl[];

typedef std::vector<webkit_glue::WebIntentServiceData> IntentServiceList;

#if !defined(ANDROID)
// Factory capable of producing a file picker NativeService.
class FilePickerFactory {
 public:
  // Returns a localized title for the file picker.
  static string16 GetServiceTitle();

  // Returns a new instance of FilePickerService.
  static IntentServiceHost* CreateServiceInstance(
      const webkit_glue::WebIntentData& intent,
      content::WebContents* web_contents);

  DISALLOW_COPY_AND_ASSIGN(FilePickerFactory);
};
#endif

class NativeServiceRegistry {
 public:
  NativeServiceRegistry();
  // Populates |services| with all supported native services.
  void GetSupportedServices(
      const string16& action,
      IntentServiceList* services);

  DISALLOW_COPY_AND_ASSIGN(NativeServiceRegistry);
};

class NativeServiceFactory {
 public:
  NativeServiceFactory();
  // Returns a NativeService instance suitable to handle |intent|.
  // |url| identifies the service to be instantiated, and |web_contents|
  // is the web_contents that may be be needed to serve as host environment
  // to the service.
  IntentServiceHost* CreateServiceInstance(
      const GURL& url,
      const webkit_glue::WebIntentData& intent,
      content::WebContents* web_contents);

  DISALLOW_COPY_AND_ASSIGN(NativeServiceFactory);
};

}  // web_intents namespaces

#endif  // CHROME_BROWSER_INTENTS_NATIVE_SERVICES_H_
