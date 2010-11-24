// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file relies on the 1.9 version of the unfrozen interfaces
// "nsIScriptSecurityManager" and "nsIScriptObjectPrincipal"
// from gecko 1.9, which means that this implementation is specific to
// FireFox 3.0 and any other browsers built from the same gecko version.
// See [http://en.wikipedia.org/wiki/Gecko_(layout_engine]
// It's a good bet that nsIScriptSecurityManager will change for gecko
// 1.9.1 and FireFox 3.5, in which case we'll need another instance of this
// code for the 3.5 version of FireFox.

#include "third_party/xulrunner-sdk/win/include/xpcom/nsIServiceManager.h"

// These are needed to work around typedef conflicts in chrome headers.
#define _UINT32
#define _INT32

#include "base/logging.h"
#include "chrome_frame/np_browser_functions.h"
#include "chrome_frame/scoped_ns_ptr_win.h"
#include "chrome_frame/ns_associate_iid_win.h"
#include "chrome_frame/np_utils.h"
#include "chrome_frame/utils.h"
#include "googleurl/src/gurl.h"

ASSOCIATE_IID(NS_ISERVICEMANAGER_IID_STR, nsIServiceManager);

// Returns true iff we're being instantiated into a document
// that has the system principal's privileges
bool IsFireFoxPrivilegedInvocation(NPP instance) {
  // For testing purposes, check the registry to see if the privilege mode
  // is being forced to a certain value.  If this property does not exist, the
  // mode should be verified normally.  If this property does exist, its value
  // is interpreted as follows:
  //
  //  0: force privilege mode off
  //  1: force privilege mode on
  //  any other value: do normal verification
  int privilege_mode = GetConfigInt(2, kEnableFirefoxPrivilegeMode);
  if (privilege_mode == 0) {
    return false;
  } else if (privilege_mode == 1) {
    return true;
  }

  // Make sure that we are running in Firefox before checking for privilege.
  const char* user_agent = npapi::UserAgent(instance);
  if (strstr(user_agent, "Firefox") == NULL)
    return false;

  ScopedNsPtr<nsIServiceManager> service_manager;
  nsresult err = NS_GetServiceManager(service_manager.Receive());
  if (NS_FAILED(err) || !service_manager.get())
    return false;

  ScopedNpObject<> window_element;
  NPError nperr = npapi::GetValue(instance,
                                  NPNVWindowNPObject,
                                  window_element.Receive());
  if (nperr != NPERR_NO_ERROR || !window_element.get())
    return false;

  std::string url_string(np_utils::GetLocation(instance, window_element));
  GURL url(url_string);

  return url.SchemeIs("chrome");
}
