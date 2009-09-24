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

// Gecko headers need this on Windows.
#define XP_WIN
#include "chrome_frame/script_security_manager.h"
#include "third_party/xulrunner-sdk/win/include/dom/nsIScriptObjectPrincipal.h"
#include "third_party/xulrunner-sdk/win/include/xpcom/nsIServiceManager.h"

// These are needed to work around typedef conflicts in chrome headers.
#define _UINT32
#define _INT32

#include "chrome_frame/np_browser_functions.h"
#include "chrome_frame/scoped_ns_ptr_win.h"
#include "chrome_frame/ns_associate_iid_win.h"
#include "base/logging.h"

ASSOCIATE_IID(NS_ISERVICEMANAGER_IID_STR, nsIServiceManager);

namespace {
// Unfortunately no NS_ISCRIPTOBJECTPRINCIPAL_IID_STR
// defined for this interface
nsIID IID_nsIScriptObjectPrincipal = NS_ISCRIPTOBJECTPRINCIPAL_IID;
}  // namespace

// Returns true iff we're being instantiated into a document
// that has the system principal's privileges
bool IsFireFoxPrivilegedInvocation(NPP instance) {
  ScopedNsPtr<nsIServiceManager> service_manager;
  NPError nperr = npapi::GetValue(instance, NPNVserviceManager,
                                  service_manager.Receive());
  if (nperr != NPERR_NO_ERROR || !service_manager.get())
    return false;
  DCHECK(service_manager);

  // Get the document.
  ScopedNsPtr<nsISupports> window;
  nperr = npapi::GetValue(instance, NPNVDOMWindow, window.Receive());
  if (nperr != NPERR_NO_ERROR || !window.get())
    return false;
  DCHECK(window);

  // This interface allows us access to the window's principal.
  ScopedNsPtr<nsIScriptObjectPrincipal, &IID_nsIScriptObjectPrincipal>
      script_object_principal;
  nsresult err = script_object_principal.QueryFrom(window);
  if (NS_FAILED(err) || !script_object_principal.get())
    return false;
  DCHECK(script_object_principal);

  // For regular HTML windows, this will be a principal encoding the
  // document's origin. For browser XUL, this will be the all-powerful
  // system principal.
  nsIPrincipal* window_principal = script_object_principal->GetPrincipal();
  DCHECK(window_principal);
  if (!window_principal)
    return false;

  // Get the script security manager.
  ScopedNsPtr<nsIScriptSecurityManager_FF35> security_manager_ff35;
  PRBool is_system = PR_FALSE;

  err = service_manager->GetServiceByContractID(
      NS_SCRIPTSECURITYMANAGER_CONTRACTID,
      nsIScriptSecurityManager_FF35::GetIID(),
      reinterpret_cast<void**>(security_manager_ff35.Receive()));
  if (NS_SUCCEEDED(err) && security_manager_ff35.get()) {
    err = security_manager_ff35->IsSystemPrincipal(window_principal,
                                                   &is_system);
    if (NS_FAILED(err))
      is_system = PR_FALSE;
  } else {
    ScopedNsPtr<nsIScriptSecurityManager_FF30> security_manager_ff30;
    err = service_manager->GetServiceByContractID(
        NS_SCRIPTSECURITYMANAGER_CONTRACTID,
        nsIScriptSecurityManager_FF30::GetIID(),
        reinterpret_cast<void**>(security_manager_ff30.Receive()));
    if (NS_SUCCEEDED(err) && security_manager_ff30.get()) {
      err = security_manager_ff30->IsSystemPrincipal(window_principal,
                                                     &is_system);
    }

    if (NS_FAILED(err))
      is_system = PR_FALSE;
  }

  return is_system == PR_TRUE;
}
