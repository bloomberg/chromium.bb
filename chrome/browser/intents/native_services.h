// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_NATIVE_SERVICES_H_
#define CHROME_BROWSER_INTENTS_NATIVE_SERVICES_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
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
// Factory capable of producing a native file picker IntentServiceHost,
// as well as producing registration information about the service.
class FilePickerFactory {
 public:
  // Returns a localized title for the file picker.
  static string16 GetServiceTitle();

  // Returns a new IntentServiceHost. The instance is owned by the caller.
  // |intent| is the intent data. |web_contents| is the context in which
  // the intent was invoked.
  static IntentServiceHost* CreateServiceInstance(
      const webkit_glue::WebIntentData& intent,
      content::WebContents* web_contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(FilePickerFactory);
};
#endif

class NativeServiceRegistry {
 public:
  NativeServiceRegistry();
  // Populates |services| with all supported IntentServiceHosts
  // capable of handling |action|.
  void GetSupportedServices(
      const string16& action,
      IntentServiceList* services);

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeServiceRegistry);
};

class NativeServiceFactory {
 public:
  NativeServiceFactory();
  // Returns an IntentServiceHost instance suitable to handle |intent|.
  // |url| identifies the service to be instantiated, |web_contents| is
  // the web_contents of the client that invoked this intent. The
  // IntentServiceHost is owned by the caller.
  IntentServiceHost* CreateServiceInstance(
      const GURL& url,
      const webkit_glue::WebIntentData& intent,
      content::WebContents* web_contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeServiceFactory);
};

}  // namespace web_intents

#endif  // CHROME_BROWSER_INTENTS_NATIVE_SERVICES_H_
