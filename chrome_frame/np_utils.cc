// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define NO_NSPR_10_SUPPORT
#include "chrome_frame/np_utils.h"

#include "chrome_frame/np_browser_functions.h"
#include "chrome_frame/ns_associate_iid_win.h"
#include "chrome_frame/scoped_ns_ptr_win.h"

#include "third_party/xulrunner-sdk/win/include/xpcom/nsXPCOM.h"
#include "third_party/xulrunner-sdk/win/include/xpcom/nsIServiceManager.h"
#include "third_party/xulrunner-sdk/win/include/necko/nsICookieService.h"
#include "third_party/xulrunner-sdk/win/include/necko/nsIIOService.h"
#include "third_party/xulrunner-sdk/win/sdk/include/nsIURI.h"
#include "third_party/xulrunner-sdk/win/sdk/include/nsStringAPI.h"

ASSOCIATE_IID(NS_ISERVICEMANAGER_IID_STR, nsIServiceManager);
namespace {
nsIID IID_nsIIOService = NS_IIOSERVICE_IID;
nsIID IID_nsICookieService = NS_ICOOKIESERVICE_IID;
nsIID IID_nsIUri = NS_IURI_IID;
const char kMozillaIOServiceContractID[] = "@mozilla.org/network/io-service;1";
const char kMozillaCookieServiceContractID[] = "@mozilla.org/cookieService;1";

bool GetXPCOMCookieServiceAndURI(NPP instance, const std::string& url,
    nsICookieService** cookie_service, nsIURI** uri) {
  DCHECK(cookie_service);
  DCHECK(uri);

  ScopedNsPtr<nsIServiceManager> service_manager;

  NPError nperr = NS_GetServiceManager(service_manager.Receive());
  if (nperr != NPERR_NO_ERROR || !service_manager.get()) {
    NPError nperr = npapi::GetValue(instance, NPNVserviceManager,
        service_manager.Receive());
    if (nperr != NPERR_NO_ERROR || !service_manager.get()) {
      return false;
    }
  }

  ScopedNsPtr<nsIIOService, &IID_nsIIOService> io_service;
  service_manager->GetServiceByContractID(
      kMozillaIOServiceContractID, nsIIOService::GetIID(),
      reinterpret_cast<void**>(io_service.Receive()));
  if (!io_service.get()) {
    NOTREACHED() << "Failed to get nsIIOService";
    return false;
  }

  ScopedNsPtr<nsICookieService, &IID_nsICookieService> nsi_cookie_service;
  service_manager->GetServiceByContractID(
      kMozillaCookieServiceContractID, nsICookieService::GetIID(),
      reinterpret_cast<void**>(nsi_cookie_service.Receive()));
  if (!io_service.get()) {
    NOTREACHED() << "Failed to get nsICookieService";
    return false;
  }

  nsCString url_string;
  url_string.Assign(url.c_str());

  ScopedNsPtr<nsIURI, &IID_nsIUri> nsi_uri;
  io_service->NewURI(url_string, NULL, NULL, nsi_uri.Receive());
  if (!nsi_uri.get()) {
    NOTREACHED() << "Failed to covert url to nsIURI";
    return false;
  }

  *cookie_service = nsi_cookie_service.Detach();
  *uri = nsi_uri.Detach();
  return true;
}

}  // namespace



namespace np_utils {

std::string GetLocation(NPP instance, NPObject* window) {
  if (!window) {
    // Can fail if the browser is closing (seen in Opera).
    return "";
  }

  std::string result;
  ScopedNpVariant href;
  ScopedNpVariant location;

  bool ok = npapi::GetProperty(instance,
                               window,
                               npapi::GetStringIdentifier("location"),
                               &location);
  DCHECK(ok);
  DCHECK(location.type == NPVariantType_Object);

  if (ok) {
    ok = npapi::GetProperty(instance,
                            location.value.objectValue,
                            npapi::GetStringIdentifier("href"),
                            &href);
    DCHECK(ok);
    DCHECK(href.type == NPVariantType_String);
    if (ok) {
      result.assign(href.value.stringValue.UTF8Characters,
                    href.value.stringValue.UTF8Length);
    }
  }

  return result;
}

bool GetCookiesUsingXPCOMCookieService(NPP instance, const std::string& url,
                                       std::string* cookie_string) {
  DCHECK(cookie_string);

  ScopedNsPtr<nsICookieService, &IID_nsICookieService> cookie_service;
  ScopedNsPtr<nsIURI, &IID_nsIUri> uri;

  if (!GetXPCOMCookieServiceAndURI(instance, url, cookie_service.Receive(),
                                   uri.Receive()))
    return false;

  nsCString cookie_value;
  nsresult ret = cookie_service->GetCookieString(uri, NULL,
                                                 getter_Copies(cookie_value));
  if (NS_SUCCEEDED(ret)) {
    *cookie_string = cookie_value.get();
  }
  return NS_SUCCEEDED(ret);
}

bool SetCookiesUsingXPCOMCookieService(NPP instance, const std::string& url,
                                       const std::string& cookie_string) {
  ScopedNsPtr<nsICookieService, &IID_nsICookieService> cookie_service;
  ScopedNsPtr<nsIURI, &IID_nsIUri> uri;

  if (!GetXPCOMCookieServiceAndURI(instance, url, cookie_service.Receive(),
                                   uri.Receive())) {
  return false;
  }

  nsCString cookie_value;
  nsresult ret = cookie_service->SetCookieString(uri, NULL,
    cookie_string.c_str(), NULL);
  return NS_SUCCEEDED(ret);
}

}  // namespace np_utils
