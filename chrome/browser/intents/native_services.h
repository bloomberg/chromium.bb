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

// Service URL for the file picker hosted by the Chrome browser.
extern const char kNativeFilePickerUrl[];

#if !defined(ANDROID)
// Factory capable of producing a native file picker IntentServiceHost,
// as well as producing registration information about the service. Instances
// of this class can be obtained via NativeServiceFactory and should not
// otherwise be instantiated directly.
class FilePickerFactory {
 public:
  // Returns a localized title for the file picker.
  static string16 GetServiceTitle();

  // Returns a new IntentServiceHost for processing the given intent data in the
  // context of the given web contents. The intent must be of action type
  // "pick".
  static IntentServiceHost* CreateServiceInstance(
      const webkit_glue::WebIntentData& intent,
      content::WebContents* web_contents);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FilePickerFactory);
};
#endif

// Supplier of information about services hosted by Chrome itself
// (as opposed to web services). Each service registration produced
// by this class will have a WebIntentServiceData::DISPOSITION_NATIVE
// disposition. This value can be used at runtime to determine when a service
// can be instantiated by our sibling class NativeServiceFactory.
// Instances of this class are currently stateless and fairly light weight.
// Any two instances can be assumed to have the same information.
class NativeServiceRegistry {
 public:
  typedef std::vector<webkit_glue::WebIntentServiceData> IntentServiceList;
  NativeServiceRegistry();
  ~NativeServiceRegistry();

  // Populates |services| with all supported IntentServiceHosts
  // capable of handling |action|.
  void GetSupportedServices(
      const string16& action,
      IntentServiceList* services);

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeServiceRegistry);
};

// Factory for services hosted by Chrome itself (as opposed to web services).
// When implementing a new native service this is where you add support
// for creating an instance.
// Only services reported by NativeServiceRegistry.GetSupportedServices,
// specifically those having a WebIntentServiceData::DISPOSITION_NATIVE
// disposition, are instantiatable via this class.
// Instances of this class are currently stateless and fairly light weight.
// Any two instances can be assumed to have the same capabilities.
class NativeServiceFactory {
 public:
  NativeServiceFactory();
  ~NativeServiceFactory();

  // Returns a new IntentServiceHost for processing the given intent data in the
  // context of the given web contents. Callers assume ownership of the
  // instance.
  IntentServiceHost* CreateServiceInstance(
      const GURL& url,
      const webkit_glue::WebIntentData& intent,
      content::WebContents* web_contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeServiceFactory);
};

}  // namespace web_intents

#endif  // CHROME_BROWSER_INTENTS_NATIVE_SERVICES_H_
