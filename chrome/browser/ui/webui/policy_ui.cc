// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_ui.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud_policy_client.h"
#include "chrome/browser/policy/cloud_policy_core.h"
#include "chrome/browser/policy/cloud_policy_refresh_scheduler.h"
#include "chrome/browser/policy/cloud_policy_store.h"
#include "chrome/browser/policy/cloud_policy_validator.h"
#include "chrome/browser/policy/message_util.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/device_local_account_policy_service.h"
#include "chrome/browser/policy/user_cloud_policy_manager_chromeos.h"
#else
#include "chrome/browser/policy/user_cloud_policy_manager.h"
#include "chrome/browser/policy/user_cloud_policy_manager_factory.h"
#endif

namespace em = enterprise_management;

const char PolicyUIHandler::kLevel[] = "level";
const char PolicyUIHandler::kName[] = "name";
const char PolicyUIHandler::kScope[] = "scope";
const char PolicyUIHandler::kSet[] = "set";
const char PolicyUIHandler::kStatus[] = "status";
const char PolicyUIHandler::kValue[] = "value";

namespace {

ChromeWebUIDataSource* CreatePolicyUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIPolicyHost);

  // Localized strings.
  source->set_use_json_js_format_v2();
  source->AddLocalizedString("policyTitle", IDS_POLICY_TITLE);
  source->AddLocalizedString("statusPaneTitle", IDS_POLICY_STATUS_TITLE);
  source->AddLocalizedString("fetchPoliciesText", IDS_POLICY_FETCH);
  source->AddLocalizedString("devicePoliciesBoxTitle",
                             IDS_POLICY_DEVICE_POLICIES);
  source->AddLocalizedString("userPoliciesBoxTitle", IDS_POLICY_USER_POLICIES);
  source->AddLocalizedString("enrollmentDomainText",
                             IDS_POLICY_ENROLLMENT_DOMAIN);
  source->AddLocalizedString("clientIdText", IDS_POLICY_CLIENT_ID);
  source->AddLocalizedString("usernameText", IDS_POLICY_USERNAME);
  source->AddLocalizedString("timeSinceLastFetchText", IDS_POLICY_LAST_FETCHED);
  source->AddLocalizedString("fetchIntervalText", IDS_POLICY_FETCH_INTERVAL);
  source->AddLocalizedString("serverStatusText", IDS_POLICY_SERVER_STATUS);
  source->AddLocalizedString("showUnsentPoliciesText", IDS_POLICY_SHOW_UNSENT);
  source->AddLocalizedString("filterPoliciesText", IDS_POLICY_FILTER);
  source->AddLocalizedString("noPoliciesSet",IDS_POLICY_NO_POLICIES_SET);
  source->AddLocalizedString("appliesToTableHeader", IDS_POLICY_APPLIES_TO);
  source->AddLocalizedString("policyLevelTableHeader", IDS_POLICY_LEVEL);
  source->AddLocalizedString("policyNameTableHeader", IDS_POLICY_ENTRY_NAME);
  source->AddLocalizedString("policyValueTableHeader", IDS_POLICY_ENTRY_VALUE);
  source->AddLocalizedString("policyStatusTableHeader",
                             IDS_POLICY_ENTRY_STATUS);
  source->AddLocalizedString("showMoreText", IDS_POLICY_SHOW_MORE);
  source->AddLocalizedString("hideText", IDS_POLICY_HIDE);
  source->set_json_path("strings.js");

  // Add required resources.
  source->add_resource_path("policy.css", IDR_POLICY_CSS);
  source->add_resource_path("policy.js", IDR_POLICY_JS);
  source->set_default_resource(IDR_POLICY_HTML);

  return source;
}

string16 GetPolicyScopeString(policy::PolicyScope scope) {
  switch (scope) {
    case policy::POLICY_SCOPE_USER:
      return l10n_util::GetStringUTF16(IDS_POLICY_SCOPE_USER);
    case policy::POLICY_SCOPE_MACHINE:
      return l10n_util::GetStringUTF16(IDS_POLICY_SCOPE_MACHINE);
  }
  NOTREACHED();
  return string16();
}

string16 GetPolicyLevelString(policy::PolicyLevel level) {
  switch (level) {
    case policy::POLICY_LEVEL_RECOMMENDED:
      return l10n_util::GetStringUTF16(IDS_POLICY_LEVEL_RECOMMENDED);
    case policy::POLICY_LEVEL_MANDATORY:
      return l10n_util::GetStringUTF16(IDS_POLICY_LEVEL_MANDATORY);
  }
  NOTREACHED();
  return string16();
}

base::DictionaryValue* GetPolicyDetails(
    const policy::PolicyDefinitionList::Entry* policy_definition,
    const policy::PolicyMap::Entry* policy_value,
    const string16& error_message) {
  base::DictionaryValue* details = new base::DictionaryValue();
  details->SetString(PolicyUIHandler::kName,
                     ASCIIToUTF16(policy_definition->name));
  details->SetBoolean(PolicyUIHandler::kSet, true);
  details->SetString(PolicyUIHandler::kLevel,
                     GetPolicyLevelString(policy_value->level));
  details->SetString(PolicyUIHandler::kScope,
                     GetPolicyScopeString(policy_value->scope));
  details->Set(PolicyUIHandler::kValue, policy_value->value->DeepCopy());
  if (error_message.empty()) {
    details->SetString(PolicyUIHandler::kStatus,
                       l10n_util::GetStringUTF16(IDS_OK));
  } else {
    details->SetString(PolicyUIHandler::kStatus, error_message);
  }
  return details;
}

base::DictionaryValue* GetPolicyErrorDetails(const std::string& policy_name,
                                             bool is_set) {
  base::DictionaryValue* details = new base::DictionaryValue();
  details->SetString(PolicyUIHandler::kName, ASCIIToUTF16(policy_name));
  details->SetBoolean(PolicyUIHandler::kSet, is_set);
  details->SetString(PolicyUIHandler::kLevel, "");
  details->SetString(PolicyUIHandler::kScope, "");
  details->SetString(PolicyUIHandler::kValue, "");
  if (is_set)
    details->SetString(PolicyUIHandler::kStatus,
                       l10n_util::GetStringUTF16(IDS_POLICY_UNKNOWN));
  else
    details->SetString(PolicyUIHandler::kStatus,
                       l10n_util::GetStringUTF16(IDS_POLICY_NOT_SET));
  return details;
}

}  // namespace

// An interface for querying status data.
class CloudPolicyStatusProvider {
 public:
  CloudPolicyStatusProvider() {}
  virtual ~CloudPolicyStatusProvider() {}

  // Sets a callback to invoke upon status changes.
  void SetStatusChangeCallback(const base::Closure& callback) {
    callback_ = callback;
  }

  // Fills |status_dict| and returns true if the status is relevant.
  virtual bool GetStatus(base::DictionaryValue* status_dict) {
    return false;
  }

 protected:
  void NotifyStatusChange() {
    if (!callback_.is_null())
      callback_.Run();
  }

 private:
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyStatusProvider);
};

// Status provider implementation that pulls cloud policy status from a
// CloudPolicyCore instance provided at construction time. Also listens for
// changes on that CloudPolicyCore and reports them through the status change
// callback.
class CloudPolicyCoreStatusProvider
    : public CloudPolicyStatusProvider,
      public policy::CloudPolicyStore::Observer {
 public:
  explicit CloudPolicyCoreStatusProvider(policy::CloudPolicyCore* core)
      : core_(core) {
    core_->store()->AddObserver(this);
  }
  virtual ~CloudPolicyCoreStatusProvider() {
    core_->store()->RemoveObserver(this);
  }

  // CloudPolicyStatusProvider:
  virtual bool GetStatus(base::DictionaryValue* status_dict) OVERRIDE {
    return GetStatusFromCore(core_, status_dict);
  }

  // policy::CloudPolicyStore::Observer:
  virtual void OnStoreLoaded(policy::CloudPolicyStore* store) OVERRIDE {
    NotifyStatusChange();
  }
  virtual void OnStoreError(policy::CloudPolicyStore* store) OVERRIDE {
    NotifyStatusChange();
  }

  // Extracts status information from |core| and writes it to |status_dict|.
  static bool GetStatusFromCore(policy::CloudPolicyCore* core,
                                base::DictionaryValue* status_dict);

 private:
  // Policy status is read from the CloudPolicyClient, CloudPolicyStore and
  // CloudPolicyRefreshScheduler hosted by this |core_|.
  policy::CloudPolicyCore* core_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyCoreStatusProvider);
};

// static
bool CloudPolicyCoreStatusProvider::GetStatusFromCore(
    policy::CloudPolicyCore* core,
    base::DictionaryValue* status_dict) {
  policy::CloudPolicyStore* store = core->store();
  policy::CloudPolicyClient* client = core->client();
  policy::CloudPolicyRefreshScheduler* refresh_scheduler =
      core->refresh_scheduler();

  string16 status;
  if (store->status() == policy::CloudPolicyStore::STATUS_OK &&
      client && client->status() != policy::DM_STATUS_SUCCESS) {
    status = policy::FormatDeviceManagementStatus(client->status());
  } else {
    status = policy::FormatStoreStatus(store->status(),
                                       store->validation_status());
  }
  status_dict->SetString("statusMessage", status);

  base::Time last_refresh_time;
  if (refresh_scheduler)
    last_refresh_time = refresh_scheduler->last_refresh();
  status_dict->SetString(
      "timeSinceLastFetch",
      last_refresh_time.is_null() ?
          l10n_util::GetStringUTF16(IDS_POLICY_NEVER_FETCHED) :
          TimeFormat::TimeElapsed(base::Time::NowFromSystemTime() -
                                  last_refresh_time));

  const em::PolicyData* policy = store->policy();
  status_dict->SetString(
      "clientId", policy ? ASCIIToUTF16(policy->device_id()) : string16());
  status_dict->SetString(
      "user", policy ? UTF8ToUTF16(policy->username()) : string16());

  int64 interval = policy::CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs;
  if (refresh_scheduler)
    interval = refresh_scheduler->refresh_delay();
  status_dict->SetString(
      "fetchInterval",
      TimeFormat::TimeRemainingShort(
          base::TimeDelta::FromMilliseconds(interval)));

  return store->is_managed();
}

#if defined(OS_CHROMEOS)
// A cloud policy status provider implementation that reads status data from a
// CloudPolicySubsystem and observes it to forward changes to the status change
// callback.
// TODO(mnissler): Remove this once the legacy cloud policy code gets retired
// (http://crbug.com/108928).
class CloudPolicySubsystemStatusProvider
    : public CloudPolicyStatusProvider,
      public policy::CloudPolicySubsystem::Observer {
 public:
  CloudPolicySubsystemStatusProvider(policy::CloudPolicySubsystem* subsystem,
                                     const std::string& refresh_pref)
      : subsystem_(subsystem),
        registrar_(subsystem, this),
        refresh_pref_(refresh_pref) {}
  virtual ~CloudPolicySubsystemStatusProvider() {}

  // CloudPolicyStatusProvider:
  virtual bool GetStatus(base::DictionaryValue* status_dict) OVERRIDE;

  // policy::CloudPolicySubsystem::Observer:
  virtual void OnPolicyStateChanged(
      policy::CloudPolicySubsystem::PolicySubsystemState state,
      policy::CloudPolicySubsystem::ErrorDetails error_details) OVERRIDE {
    NotifyStatusChange();
  }

 private:
  // The subsystem supplying status information.
  policy::CloudPolicySubsystem* subsystem_;

  // Manages the observer registration with |subsystem_|.
  policy::CloudPolicySubsystem::ObserverRegistrar registrar_;

  // The refresh interval is read from this pref key in
  // g_browser_process->local_state().
  const std::string refresh_pref_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicySubsystemStatusProvider);
};

bool CloudPolicySubsystemStatusProvider::GetStatus(
    base::DictionaryValue* status_dict) {
  static const int kStatusErrorMap[] = {
    IDS_POLICY_STATUS_OK,
    IDS_POLICY_STATUS_NETWORK_ERROR,
    IDS_POLICY_STATUS_NETWORK_ERROR,  // this is also a network error.
    IDS_POLICY_STATUS_DMTOKEN_ERROR,
    IDS_POLICY_STATUS_LOCAL_ERROR,
    IDS_POLICY_STATUS_SIGNATURE_ERROR,
    IDS_POLICY_STATUS_SERIAL_ERROR,
  };
  policy::CloudPolicySubsystem::ErrorDetails error_details =
      subsystem_->error_details();
  DCHECK(static_cast<size_t>(error_details) < arraysize(kStatusErrorMap));
  status_dict->SetString(
      "statusMessage",
      l10n_util::GetStringUTF16(kStatusErrorMap[error_details]));

  string16 time_since_last_fetch;
  base::Time last_refresh_time =
      subsystem_->GetCloudPolicyCacheBase()->last_policy_refresh_time();
  if (last_refresh_time.is_null()) {
    time_since_last_fetch =
        l10n_util::GetStringUTF16(IDS_POLICY_NEVER_FETCHED);
  } else {
    base::Time now = base::Time::NowFromSystemTime();
    time_since_last_fetch = TimeFormat::TimeElapsed(now - last_refresh_time);
  }
  status_dict->SetString("timeSinceLastFetch", time_since_last_fetch);

  policy::CloudPolicyDataStore* data_store = subsystem_->data_store();
  status_dict->SetString(
      "clientId",
      data_store ? ASCIIToUTF16(data_store->device_id()) : string16());
  status_dict->SetString(
      "user",
      data_store ? UTF8ToUTF16(data_store->user_name()) : string16());

  PrefService* prefs = g_browser_process->local_state();
  status_dict->SetString("fetchInterval",
                         TimeFormat::TimeRemainingShort(
                             base::TimeDelta::FromMilliseconds(
                                 prefs->GetInteger(refresh_pref_.c_str()))));

  return subsystem_->state() != policy::CloudPolicySubsystem::UNMANAGED;
}

// A cloud policy status provider that reads policy status from the policy core
// associated with the device-local account specified by |account_id| at
// construction time. The indirection via account ID and
// DeviceLocalAccountPolicyService is necessary because the device-local account
// may go away any time behind the scenes, at which point the status message
// text will indicate CloudPolicyStore::STATUS_BAD_STATE.
class DeviceLocalAccountPolicyStatusProvider
    : public CloudPolicyStatusProvider,
      public policy::DeviceLocalAccountPolicyService::Observer {
 public:
  DeviceLocalAccountPolicyStatusProvider(
      const std::string& account_id,
      policy::DeviceLocalAccountPolicyService* service)
      : account_id_(account_id),
        service_(service) {
    service_->AddObserver(this);
  }
  virtual ~DeviceLocalAccountPolicyStatusProvider() {
    service_->RemoveObserver(this);
  }

  // CloudPolicyStatusProvider:
  virtual bool GetStatus(base::DictionaryValue* status_dict) OVERRIDE {
    policy::DeviceLocalAccountPolicyBroker* broker =
        service_->GetBrokerForAccount(account_id_);
    if (broker) {
      return CloudPolicyCoreStatusProvider::GetStatusFromCore(broker->core(),
                                                              status_dict);
    }

    status_dict->SetString("user", account_id_);
    status_dict->SetString(
        "statusMessage",
        policy::FormatStoreStatus(
            policy::CloudPolicyStore::STATUS_BAD_STATE,
            policy::CloudPolicyValidatorBase::VALIDATION_OK));
    return true;
  }

  // policy::DeviceLocalAccountPolicyService::Observer:
  virtual void OnPolicyUpdated(const std::string& account_id) OVERRIDE {
    if (account_id == account_id_)
      NotifyStatusChange();
  }
  virtual void OnDeviceLocalAccountsChanged() OVERRIDE {
    NotifyStatusChange();
  }

 private:
  const std::string account_id_;
  policy::DeviceLocalAccountPolicyService* service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyStatusProvider);
};
#endif

////////////////////////////////////////////////////////////////////////////////
//
// PolicyUIHandler
//
////////////////////////////////////////////////////////////////////////////////

PolicyUIHandler::PolicyUIHandler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {}

PolicyUIHandler::~PolicyUIHandler() {
  GetPolicyService()->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
}

void PolicyUIHandler::RegisterMessages() {
#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  if (connector->IsEnterpriseManaged()) {
    enterprise_domain_ = UTF8ToUTF16(connector->GetEnterpriseDomain());
    policy::DeviceCloudPolicyManagerChromeOS* device_policy_manager =
        connector->GetDeviceCloudPolicyManager();
    policy::CloudPolicySubsystem* device_subsystem =
        connector->device_cloud_policy_subsystem();
    if (device_policy_manager) {
      device_status_provider_.reset(
          new CloudPolicyCoreStatusProvider(device_policy_manager->core()));
    } else if (device_subsystem) {
      device_status_provider_.reset(
          new CloudPolicySubsystemStatusProvider(
              device_subsystem, prefs::kDevicePolicyRefreshRate));
    }
  }

  chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  policy::CloudPolicyManager* user_cloud_policy_manager =
      connector->GetUserCloudPolicyManager();
  policy::CloudPolicySubsystem* user_subsystem =
      connector->user_cloud_policy_subsystem();
  if (user_manager->IsLoggedInAsPublicAccount()) {
    policy::DeviceLocalAccountPolicyService* local_account_service =
        connector->GetDeviceLocalAccountPolicyService();
    if (local_account_service) {
      user_status_provider_.reset(
          new DeviceLocalAccountPolicyStatusProvider(
              user_manager->GetLoggedInUser()->email(), local_account_service));
    }
  } else if (user_cloud_policy_manager) {
    user_status_provider_.reset(
        new CloudPolicyCoreStatusProvider(user_cloud_policy_manager->core()));
  } else if (user_subsystem) {
    user_status_provider_.reset(
        new CloudPolicySubsystemStatusProvider(
            user_subsystem, prefs::kUserPolicyRefreshRate));
  }
#else
  policy::CloudPolicyManager* user_cloud_policy_manager =
      policy::UserCloudPolicyManagerFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (user_cloud_policy_manager) {
    user_status_provider_.reset(
        new CloudPolicyCoreStatusProvider(user_cloud_policy_manager->core()));
  }
#endif

  if (!user_status_provider_.get())
    user_status_provider_.reset(new CloudPolicyStatusProvider());
  if (!device_status_provider_.get())
    device_status_provider_.reset(new CloudPolicyStatusProvider());

  base::Closure update_callback(base::Bind(&PolicyUIHandler::SendDataToUI,
                                           base::Unretained(this)));
  user_status_provider_->SetStatusChangeCallback(update_callback);
  device_status_provider_->SetStatusChangeCallback(update_callback);
  GetPolicyService()->AddObserver(policy::POLICY_DOMAIN_CHROME, this);

  web_ui()->RegisterMessageCallback(
      "requestData",
      base::Bind(&PolicyUIHandler::HandleRequestData,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "fetchPolicy",
      base::Bind(&PolicyUIHandler::HandleFetchPolicy,
                 base::Unretained(this)));
}

void PolicyUIHandler::OnPolicyUpdated(policy::PolicyDomain domain,
                                      const std::string& component_id,
                                      const policy::PolicyMap& previous,
                                      const policy::PolicyMap& current) {
  DCHECK(domain == policy::POLICY_DOMAIN_CHROME);
  DCHECK(component_id.empty());
  SendDataToUI();
}

// static
scoped_ptr<base::ListValue> PolicyUIHandler::GetPolicyStatusList(
    const policy::PolicyMap& policies,
    bool* any_policies_set) {
  scoped_ptr<base::ListValue> result(new base::ListValue());
  // Make a copy that can be modified, since some policy values are modified
  // before being displayed.
  policy::PolicyMap policy_map;
  policy_map.CopyFrom(policies);

  // Get a list of all the errors in the policy values.
  const policy::ConfigurationPolicyHandlerList* handler_list =
      g_browser_process->browser_policy_connector()->GetHandlerList();
  policy::PolicyErrorMap errors;
  handler_list->ApplyPolicySettings(policy_map, NULL, &errors);
  handler_list->PrepareForDisplaying(&policy_map);

  // Append each known policy and its status.
  *any_policies_set = false;
  std::vector<base::DictionaryValue*> unset_policies;
  base::hash_set<std::string> known_names;
  const policy::PolicyDefinitionList* policy_list =
      policy::GetChromePolicyDefinitionList();
  for (const policy::PolicyDefinitionList::Entry* policy = policy_list->begin;
       policy != policy_list->end; ++policy) {
    known_names.insert(policy->name);
    const policy::PolicyMap::Entry* entry = policy_map.Get(policy->name);
    string16 error_message = errors.GetErrors(policy->name);
    if (entry) {
      *any_policies_set = true;
      result->Append(GetPolicyDetails(policy, entry, error_message));
    } else {
      unset_policies.push_back(GetPolicyErrorDetails(policy->name, false));
    }
  }

  // Append unrecognized policy names.
  for (policy::PolicyMap::const_iterator it = policy_map.begin();
       it != policy_map.end(); ++it) {
    if (!ContainsKey(known_names, it->first))
      result->Append(GetPolicyErrorDetails(it->first, true));
  }

  // Append policies that aren't currently configured to the end.
  for (std::vector<base::DictionaryValue*>::const_iterator it =
           unset_policies.begin();
       it != unset_policies.end(); ++it) {
    result->Append(*it);
  }

  return result.Pass();
}

void PolicyUIHandler::HandleRequestData(const base::ListValue* args) {
  SendDataToUI();
}

void PolicyUIHandler::HandleFetchPolicy(const base::ListValue* args) {
  // Fetching policy can potentially take a while due to cloud policy fetches.
  // Use a WeakPtr to make sure the callback is invalidated if the tab is closed
  // before the fetching completes.
  GetPolicyService()->RefreshPolicies(
      base::Bind(&PolicyUIHandler::OnRefreshDone,
                 weak_factory_.GetWeakPtr()));
}

void PolicyUIHandler::OnRefreshDone() {
  web_ui()->CallJavascriptFunction("Policy.refreshDone");
}

void PolicyUIHandler::SendDataToUI() {
  policy::PolicyService* service = GetPolicyService();
  bool any_policies_set = false;
  base::ListValue* list =
      GetPolicyStatusList(
          service->GetPolicies(policy::POLICY_DOMAIN_CHROME, std::string()),
          &any_policies_set).release();
  base::DictionaryValue results;
  results.Set("policies", list);
  results.SetBoolean("anyPoliciesSet", any_policies_set);
  base::DictionaryValue* dict = GetStatusData();
  results.Set("status", dict);
  web_ui()->CallJavascriptFunction("Policy.returnData", results);
}

policy::PolicyService* PolicyUIHandler::GetPolicyService() {
  Profile* profile = Profile::FromWebUI(web_ui());
  return profile->GetPolicyService();
}

base::DictionaryValue* PolicyUIHandler::GetStatusData() {
  base::DictionaryValue* results = new base::DictionaryValue();

  scoped_ptr<base::DictionaryValue> user_status_dict(
      new base::DictionaryValue());
  bool user_status_available =
      user_status_provider_->GetStatus(user_status_dict.get());
  user_status_dict->SetBoolean("display", user_status_available);
  results->Set("userStatus", user_status_dict.release());

  scoped_ptr<base::DictionaryValue> device_status_dict(
      new base::DictionaryValue());
  bool device_status_available =
      device_status_provider_->GetStatus(device_status_dict.get());
  device_status_dict->SetString("domain", enterprise_domain_);
  device_status_dict->SetBoolean("display", device_status_available);
  results->Set("deviceStatus", device_status_dict.release());

  results->SetBoolean("displayStatusSection",
                      user_status_available || device_status_available);
  return results;
}

////////////////////////////////////////////////////////////////////////////////
//
// PolicyUI
//
////////////////////////////////////////////////////////////////////////////////

PolicyUI::PolicyUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new PolicyUIHandler);

  // Set up the chrome://policy/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSourceImpl(profile, CreatePolicyUIHTMLSource());
}

PolicyUI::~PolicyUI() {
}
