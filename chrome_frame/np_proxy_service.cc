// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_number_conversions.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome_frame/np_proxy_service.h"
#include "chrome_frame/np_browser_functions.h"

#include "net/proxy/proxy_config.h"

#include "third_party/xulrunner-sdk/win/include/xpcom/nsXPCOM.h"
#include "third_party/xulrunner-sdk/win/include/xpcom/nsIObserverService.h"
#include "third_party/xulrunner-sdk/win/include/xpcom/nsISupportsUtils.h"
#include "third_party/xulrunner-sdk/win/include/xpcom/nsStringAPI.h"

ASSOCIATE_IID(NS_IOBSERVERSERVICE_IID_STR, nsIObserverService);
ASSOCIATE_IID(NS_IPREFBRANCH_IID_STR, nsIPrefBranch);

// Firefox preference names.
const char* kProxyObserverRoot = "network.";
const char* kProxyObserverBranch = "proxy.";
const char* kProxyType = "proxy.type";
const char* kProxyAutoconfigUrl = "proxy.autoconfig_url";
const char* kProxyBypassList = "proxy.no_proxies_on";

const int kInvalidIntPref = -1;

// These are the proxy schemes that Chrome knows about at the moment.
// SOCKS is a notable ommission here, this will need to be updated when
// Chrome supports SOCKS proxies.
const NpProxyService::ProxyNames NpProxyService::kProxyInfo[] = {
    {"http", "proxy.http", "proxy.http_port"},
    {"https", "proxy.ssl", "proxy.ssl_port"},
    {"ftp", "proxy.ftp", "proxy.ftp_port"} };

NpProxyService::NpProxyService(void)
    : type_(PROXY_CONFIG_LAST), auto_detect_(false), no_proxy_(false),
      system_config_(false), automation_client_(NULL) {
}

NpProxyService::~NpProxyService(void) {
}

bool NpProxyService::Initialize(NPP instance,
    ChromeFrameAutomationClient* automation_client) {
  DCHECK(automation_client);
  automation_client_ = automation_client;

  // Get the pref service
  bool result = false;
  ScopedNsPtr<nsISupports> service_manager_base;
  npapi::GetValue(instance, NPNVserviceManager, service_manager_base.Receive());
  if (service_manager_base != NULL) {
    service_manager_.QueryFrom(service_manager_base);
    if (service_manager_.get() == NULL) {
      DLOG(ERROR) << "Failed to create ServiceManager. This only works in FF.";
    } else {
      service_manager_->GetServiceByContractID(
          NS_PREFSERVICE_CONTRACTID, NS_GET_IID(nsIPrefService),
          reinterpret_cast<void**>(pref_service_.Receive()));
      if (!pref_service_) {
        DLOG(ERROR) << "Failed to create PreferencesService";
      } else {
        result = InitializePrefBranch(pref_service_);
      }
    }
  }
  return result;
}

bool NpProxyService::InitializePrefBranch(nsIPrefService* pref_service) {
  DCHECK(pref_service);
  // Note that we cannot persist a reference to the pref branch because we
  // also act as an observer of changes to the branch. As per
  // nsIPrefBranch2.h, this would result in a circular reference between us
  // and the pref branch, which can impede cleanup. There are workarounds,
  // but let's try just not caching the branch reference for now.
  bool result = false;
  ScopedNsPtr<nsIPrefBranch> pref_branch;

  pref_service->GetBranch(kProxyObserverRoot, pref_branch.Receive());

  if (!pref_branch) {
    DLOG(ERROR) << "Failed to get nsIPrefBranch";
  } else {
    if (!ReadProxySettings(pref_branch.get())) {
      DLOG(ERROR) << "Could not read proxy settings.";
    } else {
      observer_pref_branch_.QueryFrom(pref_branch);
      if (!observer_pref_branch_) {
        DLOG(ERROR) << "Failed to get observer nsIPrefBranch2";
      } else {
        nsresult res = observer_pref_branch_->AddObserver(kProxyObserverBranch,
                                                          this, PR_FALSE);
        result = NS_SUCCEEDED(res);
      }
    }
  }
  return result;
}

bool NpProxyService::UnInitialize() {
  // Fail early if this was never created - we may not be running on FF.
  if (!pref_service_)
    return false;

  // Unhook ourselves as an observer.
  nsresult res = NS_ERROR_FAILURE;
  if (observer_pref_branch_)
    res = observer_pref_branch_->RemoveObserver(kProxyObserverBranch, this);

  return NS_SUCCEEDED(res);
}

NS_IMETHODIMP NpProxyService::Observe(nsISupports* subject, const char* topic,
                                     const PRUnichar* data) {
  if (!subject || !topic) {
    NOTREACHED();
    return NS_ERROR_UNEXPECTED;
  }

  std::string topic_str(topic);
  nsresult res = NS_OK;
  if (topic_str == NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) {
    // Looks like our proxy settings changed. We need to reload!
    // I have observed some extremely strange behaviour here. Specifically,
    // we are supposed to be able to QI |subject| and get from it an
    // nsIPrefBranch from which we can query new values. This has erratic
    // behaviour, specifically subject starts returning null on all member
    // queries. So I am using the cached nsIPrefBranch2 (that we used to add
    // the observer) to do the querying.
    if (NS_SUCCEEDED(res)) {
      if (!ReadProxySettings(observer_pref_branch_)) {
        res = NS_ERROR_UNEXPECTED;
      } else {
        std::string proxy_settings;
        if (GetProxyValueJSONString(&proxy_settings))
          automation_client_->SetProxySettings(proxy_settings);
      }
    }
  } else {
    NOTREACHED();
  }

  return res;
}

std::string NpProxyService::GetStringPref(nsIPrefBranch* pref_branch,
                                         const char* pref_name) {
  nsCString pref_string;
  std::string result;
  nsresult rv = pref_branch->GetCharPref(pref_name, getter_Copies(pref_string));
  if (SUCCEEDED(rv) && pref_string.get()) {
    result = pref_string.get();
  }
  return result;
}

int NpProxyService::GetIntPref(nsIPrefBranch* pref_branch,
                              const char* pref_name) {
  PRInt32 pref_int;
  int result = kInvalidIntPref;
  nsresult rv = pref_branch->GetIntPref(pref_name, &pref_int);
  if (SUCCEEDED(rv)) {
    result = pref_int;
  }
  return result;
}

bool NpProxyService::GetBoolPref(nsIPrefBranch* pref_branch,
                                const char* pref_name) {
  PRBool pref_bool;
  bool result = false;
  nsresult rv = pref_branch->GetBoolPref(pref_name, &pref_bool);
  if (SUCCEEDED(rv)) {
    result = pref_bool == PR_TRUE;
  }
  return result;
}

void NpProxyService::Reset() {
  type_ = PROXY_CONFIG_LAST;
  auto_detect_ = false;
  no_proxy_ = false;
  system_config_ = false;
  manual_proxies_.clear();
  pac_url_.clear();
  proxy_bypass_list_.clear();
}

bool NpProxyService::ReadProxySettings(nsIPrefBranch* pref_branch) {
  DCHECK(pref_branch);

  // Clear our current settings.
  Reset();
  type_ = GetIntPref(pref_branch, kProxyType);
  if (type_ == kInvalidIntPref) {
    NOTREACHED();
    return false;
  }

  switch (type_) {
    case PROXY_CONFIG_DIRECT:
    case PROXY_CONFIG_DIRECT4X:
      no_proxy_ = true;
      break;
    case PROXY_CONFIG_SYSTEM:
      // _SYSTEM is documented as "Use system settings if available, otherwise
      // DIRECT". It isn't clear under what circumstances system settings would
      // be unavailable, but I'll special-case this nonetheless and have
      // GetProxyValueJSONString() return empty if we get this proxy type.
      DLOG(WARNING) << "Received PROXY_CONFIG_SYSTEM proxy type.";
      system_config_ = true;
      break;
    case PROXY_CONFIG_WPAD:
      auto_detect_ = true;
      break;
    case PROXY_CONFIG_PAC:
      pac_url_ = GetStringPref(pref_branch, kProxyAutoconfigUrl);
      break;
    case PROXY_CONFIG_MANUAL:
      // Read in the values for each of the known schemes.
      for (int i = 0; i < arraysize(kProxyInfo); i++) {
        ManualProxyEntry entry;
        entry.url = GetStringPref(pref_branch, kProxyInfo[i].pref_name);
        entry.port = GetIntPref(pref_branch, kProxyInfo[i].port_pref_name);
        if (!entry.url.empty() && entry.port != kInvalidIntPref) {
          entry.scheme = kProxyInfo[i].chrome_scheme;
          manual_proxies_.push_back(entry);
        }
      }

      // Also pick up the list of URLs we bypass proxies for.
      proxy_bypass_list_ = GetStringPref(pref_branch, kProxyBypassList);
      break;
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

DictionaryValue* NpProxyService::BuildProxyValueSet() {
  scoped_ptr<DictionaryValue> proxy_settings_value(new DictionaryValue);

  if (auto_detect_) {
    proxy_settings_value->SetBoolean(automation::kJSONProxyAutoconfig,
                                     auto_detect_);
  }

  if (no_proxy_) {
    proxy_settings_value->SetBoolean(automation::kJSONProxyNoProxy, no_proxy_);
  }

  if (!pac_url_.empty()) {
    proxy_settings_value->SetString(automation::kJSONProxyPacUrl, pac_url_);
  }

  if (!proxy_bypass_list_.empty()) {
    proxy_settings_value->SetString(automation::kJSONProxyBypassList,
                                    proxy_bypass_list_);
  }

  // Fill in the manual proxy settings. Build a string representation that
  // corresponds to the format of the input parameter to
  // ProxyConfig::ProxyRules::ParseFromString.
  std::string manual_proxy_settings;
  ManualProxyList::const_iterator iter(manual_proxies_.begin());
  for (; iter != manual_proxies_.end(); iter++) {
    DCHECK(!iter->scheme.empty());
    DCHECK(!iter->url.empty());
    DCHECK(iter->port != kInvalidIntPref);
    manual_proxy_settings += iter->scheme;
    manual_proxy_settings += "=";
    manual_proxy_settings += iter->url;
    manual_proxy_settings += ":";
    manual_proxy_settings += base::IntToString(iter->port);
    manual_proxy_settings += ";";
  }

  if (!manual_proxy_settings.empty()) {
    proxy_settings_value->SetString(automation::kJSONProxyServer,
                                    manual_proxy_settings);
  }

  return proxy_settings_value.release();
}

bool NpProxyService::GetProxyValueJSONString(std::string* output) {
  DCHECK(output);
  output->empty();

  // If we detected a PROXY_CONFIG_SYSTEM config type or failed to obtain the
  // pref service then return false here to make Chrome continue using its
  // default proxy settings.
  if (system_config_ || !pref_service_)
    return false;

  scoped_ptr<DictionaryValue> proxy_settings_value(BuildProxyValueSet());

  JSONStringValueSerializer serializer(output);
  return serializer.Serialize(*static_cast<Value*>(proxy_settings_value.get()));
}
