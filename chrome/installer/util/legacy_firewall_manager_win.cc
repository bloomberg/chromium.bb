// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/legacy_firewall_manager_win.h"

#include "base/logging.h"
#include "base/win/scoped_bstr.h"

namespace installer {

LegacyFirewallManager::LegacyFirewallManager() {}

LegacyFirewallManager::~LegacyFirewallManager() {}

bool LegacyFirewallManager::Init(const base::string16& app_name,
                                 const base::FilePath& app_path) {
  base::win::ScopedComPtr<INetFwMgr> firewall_manager;
  HRESULT hr = firewall_manager.CreateInstance(CLSID_NetFwMgr);
  if (FAILED(hr)) {
    DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
    return false;
  }

  base::win::ScopedComPtr<INetFwPolicy> firewall_policy;
  hr = firewall_manager->get_LocalPolicy(firewall_policy.Receive());
  if (FAILED(hr)) {
    DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
    return false;
  }

  hr = firewall_policy->get_CurrentProfile(current_profile_.Receive());
  if (FAILED(hr)) {
    DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
    current_profile_ = NULL;
    return false;
  }

  app_name_ = app_name;
  app_path_ = app_path;
  return true;
}

bool LegacyFirewallManager::IsFirewallEnabled() {
  VARIANT_BOOL is_enabled = VARIANT_TRUE;
  HRESULT hr = current_profile_->get_FirewallEnabled(&is_enabled);
  return SUCCEEDED(hr) && is_enabled != VARIANT_FALSE;
}

bool LegacyFirewallManager::GetAllowIncomingConnection(bool* value) {
  // Otherwise, check to see if there is a rule either allowing or disallowing
  // this chrome.exe.
  base::win::ScopedComPtr<INetFwAuthorizedApplications> authorized_apps(
      GetAuthorizedApplications());
  if (!authorized_apps.get())
    return false;

  base::win::ScopedComPtr<INetFwAuthorizedApplication> chrome_application;
  HRESULT hr = authorized_apps->Item(
      base::win::ScopedBstr(app_path_.value().c_str()),
      chrome_application.Receive());
  if (FAILED(hr))
    return false;
  VARIANT_BOOL is_enabled = VARIANT_FALSE;
  hr = chrome_application->get_Enabled(&is_enabled);
  if (FAILED(hr))
    return false;
  if (value)
    *value = (is_enabled == VARIANT_TRUE);
  return true;
}

// The SharedAccess service must be running.
bool LegacyFirewallManager::SetAllowIncomingConnection(bool allow) {
  base::win::ScopedComPtr<INetFwAuthorizedApplications> authorized_apps(
      GetAuthorizedApplications());
  if (!authorized_apps.get())
    return false;

  // Authorize chrome.
  base::win::ScopedComPtr<INetFwAuthorizedApplication> authorization =
      CreateChromeAuthorization(allow);
  if (!authorization.get())
    return false;
  HRESULT hr = authorized_apps->Add(authorization.get());
  DLOG_IF(ERROR, FAILED(hr)) << logging::SystemErrorCodeToString(hr);
  return SUCCEEDED(hr);
}

void LegacyFirewallManager::DeleteRule() {
  base::win::ScopedComPtr<INetFwAuthorizedApplications> authorized_apps(
      GetAuthorizedApplications());
  if (!authorized_apps.get())
    return;
  authorized_apps->Remove(base::win::ScopedBstr(app_path_.value().c_str()));
}

base::win::ScopedComPtr<INetFwAuthorizedApplications>
LegacyFirewallManager::GetAuthorizedApplications() {
  base::win::ScopedComPtr<INetFwAuthorizedApplications> authorized_apps;
  HRESULT hr =
      current_profile_->get_AuthorizedApplications(authorized_apps.Receive());
  if (FAILED(hr)) {
    DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
    return base::win::ScopedComPtr<INetFwAuthorizedApplications>();
  }

  return authorized_apps;
}

base::win::ScopedComPtr<INetFwAuthorizedApplication>
LegacyFirewallManager::CreateChromeAuthorization(bool allow) {
  base::win::ScopedComPtr<INetFwAuthorizedApplication> chrome_application;

  HRESULT hr =
      chrome_application.CreateInstance(CLSID_NetFwAuthorizedApplication);
  if (FAILED(hr)) {
    DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
    return base::win::ScopedComPtr<INetFwAuthorizedApplication>();
  }

  chrome_application->put_Name(base::win::ScopedBstr(app_name_.c_str()));
  chrome_application->put_ProcessImageFileName(
      base::win::ScopedBstr(app_path_.value().c_str()));
  // IpVersion defaults to NET_FW_IP_VERSION_ANY.
  // Scope defaults to NET_FW_SCOPE_ALL.
  // RemoteAddresses defaults to "*".
  chrome_application->put_Enabled(allow ? VARIANT_TRUE : VARIANT_FALSE);

  return chrome_application;
}

}  // namespace installer
