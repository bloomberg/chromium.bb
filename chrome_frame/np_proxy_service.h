// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_NP_PROXY_SERVICE_H_
#define CHROME_FRAME_NP_PROXY_SERVICE_H_

#include <string>
#include <vector>
#include "base/values.h"
#include "base/scoped_ptr.h"

// Avoid conflicts with basictypes and the gecko sdk.
// (different definitions of uint32).
#define NO_NSPR_10_SUPPORT

#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/ns_associate_iid_win.h"
#include "chrome_frame/ns_isupports_impl.h"
#include "chrome_frame/scoped_ns_ptr_win.h"
#include "third_party/WebKit/WebCore/bridge/npapi.h"
#include "third_party/xulrunner-sdk/win/include/xpcom/nsIObserver.h"
#include "third_party/xulrunner-sdk/win/include/pref/nsIPrefBranch2.h"
#include "third_party/xulrunner-sdk/win/include/pref/nsIPrefService.h"
#include "third_party/xulrunner-sdk/win/include/xpcom/nsIServiceManager.h"

ASSOCIATE_IID(NS_IOBSERVER_IID_STR, nsIObserver);
ASSOCIATE_IID(NS_ISERVICEMANAGER_IID_STR, nsIServiceManager);
ASSOCIATE_IID(NS_IPREFSERVICE_IID_STR, nsIPrefService);
ASSOCIATE_IID(NS_IPREFBRANCH2_IID_STR, nsIPrefBranch2);

class nsIServiceManager;
class nsIPrefService;
class nsIPrefBranch2;

// This class reads in proxy settings from firefox.
// TODO(robertshield): The change notification code is currently broken.
// Fix it and implement calling back through to the automation proxy with
// proxy updates.
class NpProxyService : public NsISupportsImplBase<NpProxyService>,
                       public nsIObserver {
 public:
  // These values correspond to the integer network.proxy.type preference.
  enum ProxyConfigType {
    PROXY_CONFIG_DIRECT,
    PROXY_CONFIG_MANUAL,
    PROXY_CONFIG_PAC,
    PROXY_CONFIG_DIRECT4X,
    PROXY_CONFIG_WPAD,
    PROXY_CONFIG_SYSTEM,  // use system settings if available, otherwise DIRECT
    PROXY_CONFIG_LAST
  };

  // nsISupports
  NS_IMETHODIMP_(nsrefcnt) AddRef(void) {
    return NsISupportsImplBase<NpProxyService>::AddRef();
  }

  NS_IMETHODIMP_(nsrefcnt) Release(void) {
    return NsISupportsImplBase<NpProxyService>::Release();
  }

  NS_IMETHOD QueryInterface(REFNSIID iid, void** ptr) {
    nsresult res =
        NsISupportsImplBase<NpProxyService>::QueryInterface(iid, ptr);
    if (NS_FAILED(res) &&
        memcmp(&iid, &__uuidof(nsIObserver), sizeof(nsIID)) == 0) {
      *ptr = static_cast<nsIObserver*>(this);
      AddRef();
      res = NS_OK;
    }
    return res;
  }

  // nsIObserver
  NS_IMETHOD Observe(nsISupports* subject, const char* topic,
                     const PRUnichar* data);

  NpProxyService();
  ~NpProxyService();

  virtual bool Initialize(NPP instance,
                          ChromeFrameAutomationClient* automation_client);
  bool UnInitialize();

  // Places the current Firefox settings as a JSON string suitable for posting
  // over to Chromium into output. Returns true if the settings were correctly
  // serialized into a JSON string, false otherwise.
  // TODO(robertshield): I haven't yet nailed down how much of this should go
  // here and how much should go in the AutomationProxy. Will do that in a
  // near-future patch.
  bool GetProxyValueJSONString(std::string* output);

 private:
  bool InitializePrefBranch(nsIPrefService* pref_service);
  bool ReadProxySettings(nsIPrefBranch* pref_branch);

  std::string GetStringPref(nsIPrefBranch* pref_branch, const char* pref_name);
  int GetIntPref(nsIPrefBranch* pref_branch, const char* pref_name);
  bool GetBoolPref(nsIPrefBranch* pref_branch, const char* pref_name);

  void Reset();
  DictionaryValue* BuildProxyValueSet();

  scoped_refptr<ChromeFrameAutomationClient> automation_client_;

  ScopedNsPtr<nsIServiceManager> service_manager_;
  ScopedNsPtr<nsIPrefService> pref_service_;
  ScopedNsPtr<nsIPrefBranch2> observer_pref_branch_;

  struct ProxyNames {
    // Proxy type (http, https, ftp, etc...).
    const char* chrome_scheme;
    // Firefox preference name of the URL for this proxy type.
    const char* pref_name;
    // Firefox preference name for the port for this proxy type.
    const char* port_pref_name;
  };
  static const ProxyNames kProxyInfo[];

  struct ManualProxyEntry {
    std::string scheme;
    std::string url;
    int port;
  };
  typedef std::vector<ManualProxyEntry> ManualProxyList;

  bool system_config_;
  bool auto_detect_;
  bool no_proxy_;
  int type_;
  std::string pac_url_;
  std::string proxy_bypass_list_;
  ManualProxyList manual_proxies_;
};

#endif  // CHROME_FRAME_NP_PROXY_SERVICE_H_
