// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/cloud_policy_core.h"
#include "chrome/browser/policy/cloud/cloud_policy_refresh_scheduler.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"
#include "chrome/browser/policy/cloud/cloud_policy_validator.h"
#include "chrome/browser/policy/cloud/message_util.h"
#include "chrome/browser/policy/configuration_policy_handler_list.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/policy/proto/cloud/device_management_backend.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#endif

namespace em = enterprise_management;

namespace {

content::WebUIDataSource* CreatePolicyUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPolicyHost);

  // Localized strings.
  source->AddLocalizedString("title", IDS_POLICY_TITLE);
  source->AddLocalizedString("filterPlaceholder",
                             IDS_POLICY_FILTER_PLACEHOLDER);
  source->AddLocalizedString("reloadPolicies", IDS_POLICY_RELOAD_POLICIES);
  source->AddLocalizedString("status", IDS_POLICY_STATUS);
  source->AddLocalizedString("statusDevice", IDS_POLICY_STATUS_DEVICE);
  source->AddLocalizedString("statusUser", IDS_POLICY_STATUS_USER);
  source->AddLocalizedString("labelDomain", IDS_POLICY_LABEL_DOMAIN);
  source->AddLocalizedString("labelUsername", IDS_POLICY_LABEL_USERNAME);
  source->AddLocalizedString("labelClientId", IDS_POLICY_LABEL_CLIENT_ID);
  source->AddLocalizedString("labelTimeSinceLastRefresh",
                             IDS_POLICY_LABEL_TIME_SINCE_LAST_REFRESH);
  source->AddLocalizedString("labelRefreshInterval",
                             IDS_POLICY_LABEL_REFRESH_INTERVAL);
  source->AddLocalizedString("labelStatus", IDS_POLICY_LABEL_STATUS);
  source->AddLocalizedString("showUnset", IDS_POLICY_SHOW_UNSET);
  source->AddLocalizedString("noPoliciesSet", IDS_POLICY_NO_POLICIES_SET);
  source->AddLocalizedString("headerScope", IDS_POLICY_HEADER_SCOPE);
  source->AddLocalizedString("headerLevel", IDS_POLICY_HEADER_LEVEL);
  source->AddLocalizedString("headerName", IDS_POLICY_HEADER_NAME);
  source->AddLocalizedString("headerValue", IDS_POLICY_HEADER_VALUE);
  source->AddLocalizedString("headerStatus", IDS_POLICY_HEADER_STATUS);
  source->AddLocalizedString("showExpandedValue",
                             IDS_POLICY_SHOW_EXPANDED_VALUE);
  source->AddLocalizedString("hideExpandedValue",
                             IDS_POLICY_HIDE_EXPANDED_VALUE);
  source->AddLocalizedString("scopeUser", IDS_POLICY_SCOPE_USER);
  source->AddLocalizedString("scopeDevice", IDS_POLICY_SCOPE_DEVICE);
  source->AddLocalizedString("levelRecommended", IDS_POLICY_LEVEL_RECOMMENDED);
  source->AddLocalizedString("levelMandatory", IDS_POLICY_LEVEL_MANDATORY);
  source->AddLocalizedString("ok", IDS_POLICY_OK);
  source->AddLocalizedString("unset", IDS_POLICY_UNSET);
  source->AddLocalizedString("unknown", IDS_POLICY_UNKNOWN);

  source->SetUseJsonJSFormatV2();
  source->SetJsonPath("strings.js");

  // Add required resources.
  source->AddResourcePath("policy.css", IDR_POLICY_CSS);
  source->AddResourcePath("policy.js", IDR_POLICY_JS);
  source->AddResourcePath("uber_utils.js", IDR_UBER_UTILS_JS);
  source->SetDefaultResource(IDR_POLICY_HTML);

  return source;
}

void GetStatusFromCore(const policy::CloudPolicyCore* core,
                       base::DictionaryValue* dict) {
  const policy::CloudPolicyStore* store = core->store();
  const policy::CloudPolicyClient* client = core->client();
  const policy::CloudPolicyRefreshScheduler* refresh_scheduler =
        core->refresh_scheduler();

  bool no_error = store->status() == policy::CloudPolicyStore::STATUS_OK &&
                  client && client->status() == policy::DM_STATUS_SUCCESS;
  string16 status = store->status() == policy::CloudPolicyStore::STATUS_OK &&
                    client && client->status() != policy::DM_STATUS_SUCCESS ?
                        policy::FormatDeviceManagementStatus(client->status()) :
                        policy::FormatStoreStatus(store->status(),
                                                  store->validation_status());
  const em::PolicyData* policy = store->policy();
  std::string client_id = policy ? policy->device_id() : std::string();
  std::string username = policy ? policy->username() : std::string();
  base::TimeDelta refresh_interval =
      base::TimeDelta::FromMilliseconds(refresh_scheduler ?
          refresh_scheduler->refresh_delay() :
          policy::CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
  base::Time last_refresh_time = refresh_scheduler ?
      refresh_scheduler->last_refresh() : base::Time();

  dict->SetBoolean("error", !no_error);
  dict->SetString("status", status);
  dict->SetString("clientId", client_id);
  dict->SetString("username", username);
  dict->SetString("refreshInterval",
                  TimeFormat::TimeRemainingShort(refresh_interval));
  dict->SetString("timeSinceLastRefresh", last_refresh_time.is_null() ?
      l10n_util::GetStringUTF16(IDS_POLICY_NEVER_FETCHED) :
      TimeFormat::TimeElapsed(base::Time::NowFromSystemTime() -
                              last_refresh_time));
}

void ExtractDomainFromUsername(base::DictionaryValue* dict) {
  std::string username;
  dict->GetString("username", &username);
  if (!username.empty())
    dict->SetString("domain", gaia::ExtractDomainName(username));
}

}  // namespace

// An interface for querying the status of cloud policy.
class CloudPolicyStatusProvider {
 public:
  CloudPolicyStatusProvider();
  virtual ~CloudPolicyStatusProvider();

  // Sets a callback to invoke upon status changes.
  void SetStatusChangeCallback(const base::Closure& callback);

  virtual void GetStatus(base::DictionaryValue* dict);

 protected:
  void NotifyStatusChange();

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
  explicit CloudPolicyCoreStatusProvider(policy::CloudPolicyCore* core);
  virtual ~CloudPolicyCoreStatusProvider();

  // policy::CloudPolicyStore::Observer implementation.
  virtual void OnStoreLoaded(policy::CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(policy::CloudPolicyStore* store) OVERRIDE;

 protected:
  // Policy status is read from the CloudPolicyClient, CloudPolicyStore and
  // CloudPolicyRefreshScheduler hosted by this |core_|.
  policy::CloudPolicyCore* core_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyCoreStatusProvider);
};

// A cloud policy status provider for user policy.
class UserPolicyStatusProvider : public CloudPolicyCoreStatusProvider {
 public:
  explicit UserPolicyStatusProvider(policy::CloudPolicyCore* core);
  virtual ~UserPolicyStatusProvider();

  // CloudPolicyCoreStatusProvider implementation.
  virtual void GetStatus(base::DictionaryValue* dict) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserPolicyStatusProvider);
};

#if defined(OS_CHROMEOS)
// A cloud policy status provider for device policy.
class DevicePolicyStatusProvider : public CloudPolicyCoreStatusProvider {
 public:
  explicit DevicePolicyStatusProvider(
      policy::BrowserPolicyConnector* connector);
  virtual ~DevicePolicyStatusProvider();

  // CloudPolicyCoreStatusProvider implementation.
  virtual void GetStatus(base::DictionaryValue* dict) OVERRIDE;

 private:
  std::string domain_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyStatusProvider);
};

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
      policy::DeviceLocalAccountPolicyService* service);
  virtual ~DeviceLocalAccountPolicyStatusProvider();

  // CloudPolicyStatusProvider implementation.
  virtual void GetStatus(base::DictionaryValue* dict) OVERRIDE;

  // policy::DeviceLocalAccountPolicyService::Observer implementation.
  virtual void OnPolicyUpdated(const std::string& account_id) OVERRIDE;
  virtual void OnDeviceLocalAccountsChanged() OVERRIDE;

 private:
  const std::string account_id_;
  policy::DeviceLocalAccountPolicyService* service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyStatusProvider);
};
#endif

// The JavaScript message handler for the chrome://policy page.
class PolicyUIHandler : public content::WebUIMessageHandler,
                        public policy::PolicyService::Observer {
 public:
  PolicyUIHandler();
  virtual ~PolicyUIHandler();

  // content::WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // policy::PolicyService::Observer implementation.
  virtual void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                               const policy::PolicyMap& previous,
                               const policy::PolicyMap& current) OVERRIDE;

 private:
  // Send a dictionary containing the names of all known policies to the UI.
  void SendPolicyNames() const;

  // Send information about the current policy values to the UI. For each policy
  // whose value has been set, a dictionary containing the value and additional
  // metadata is sent.
  void SendPolicyValues() const;

  // Send the status of cloud policy to the UI. For each scope that has cloud
  // policy enabled (device and/or user), a dictionary containing status
  // information is sent.
  void SendStatus() const;

  void HandleInitialized(const base::ListValue* args);
  void HandleReloadPolicies(const base::ListValue* args);

  void OnRefreshPoliciesDone() const;

  policy::PolicyService* GetPolicyService() const;

  bool initialized_;
  std::string device_domain_;
  base::WeakPtrFactory<PolicyUIHandler> weak_factory_;

  // Providers that supply status dictionaries for user and device policy,
  // respectively. These are created on initialization time as appropriate for
  // the platform (Chrome OS / desktop) and type of policy that is in effect.
  scoped_ptr<CloudPolicyStatusProvider> user_status_provider_;
  scoped_ptr<CloudPolicyStatusProvider> device_status_provider_;

  DISALLOW_COPY_AND_ASSIGN(PolicyUIHandler);
};

CloudPolicyStatusProvider::CloudPolicyStatusProvider() {
}

CloudPolicyStatusProvider::~CloudPolicyStatusProvider() {
}

void CloudPolicyStatusProvider::SetStatusChangeCallback(
    const base::Closure& callback) {
  callback_ = callback;
}

void CloudPolicyStatusProvider::GetStatus(base::DictionaryValue* dict) {
}

void CloudPolicyStatusProvider::NotifyStatusChange() {
  if (!callback_.is_null())
    callback_.Run();
}

CloudPolicyCoreStatusProvider::CloudPolicyCoreStatusProvider(
    policy::CloudPolicyCore* core) : core_(core) {
  core_->store()->AddObserver(this);
  // TODO(bartfab): Add an observer that watches for client errors. Observing
  // core_->client() directly is not safe as the client may be destroyed and
  // (re-)created anytime if the user signs in or out on desktop platforms.
}

CloudPolicyCoreStatusProvider::~CloudPolicyCoreStatusProvider() {
  core_->store()->RemoveObserver(this);
}

void CloudPolicyCoreStatusProvider::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

void CloudPolicyCoreStatusProvider::OnStoreError(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

UserPolicyStatusProvider::UserPolicyStatusProvider(
    policy::CloudPolicyCore* core) : CloudPolicyCoreStatusProvider(core) {
}

UserPolicyStatusProvider::~UserPolicyStatusProvider() {
}

void UserPolicyStatusProvider::GetStatus(base::DictionaryValue* dict) {
  if (!core_->store()->is_managed())
    return;
  GetStatusFromCore(core_, dict);
  ExtractDomainFromUsername(dict);
}

#if defined(OS_CHROMEOS)
DevicePolicyStatusProvider::DevicePolicyStatusProvider(
    policy::BrowserPolicyConnector* connector)
      : CloudPolicyCoreStatusProvider(
            connector->GetDeviceCloudPolicyManager()->core()) {
  domain_ = connector->GetEnterpriseDomain();
}

DevicePolicyStatusProvider::~DevicePolicyStatusProvider() {
}

void DevicePolicyStatusProvider::GetStatus(base::DictionaryValue* dict) {
  GetStatusFromCore(core_, dict);
  dict->SetString("domain", domain_);
}

DeviceLocalAccountPolicyStatusProvider::DeviceLocalAccountPolicyStatusProvider(
    const std::string& account_id,
    policy::DeviceLocalAccountPolicyService* service)
      : account_id_(account_id),
        service_(service) {
  service_->AddObserver(this);
}

DeviceLocalAccountPolicyStatusProvider::
    ~DeviceLocalAccountPolicyStatusProvider() {
  service_->RemoveObserver(this);
}

void DeviceLocalAccountPolicyStatusProvider::GetStatus(
    base::DictionaryValue* dict) {
  const policy::DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForAccount(account_id_);
  if (broker) {
    GetStatusFromCore(broker->core(), dict);
  } else {
    dict->SetBoolean("error", true);
    dict->SetString("status",
                    policy::FormatStoreStatus(
                        policy::CloudPolicyStore::STATUS_BAD_STATE,
                        policy::CloudPolicyValidatorBase::VALIDATION_OK));
    dict->SetString("username", account_id_);
  }
  ExtractDomainFromUsername(dict);
  dict->SetBoolean("publicAccount", true);
}

void DeviceLocalAccountPolicyStatusProvider::OnPolicyUpdated(
    const std::string& account_id) {
  if (account_id == account_id_)
    NotifyStatusChange();
}

void DeviceLocalAccountPolicyStatusProvider::OnDeviceLocalAccountsChanged() {
  NotifyStatusChange();
}
#endif

PolicyUIHandler::PolicyUIHandler()
    : weak_factory_(this) {
}

PolicyUIHandler::~PolicyUIHandler() {
  GetPolicyService()->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
}

void PolicyUIHandler::RegisterMessages() {
#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  if (connector->IsEnterpriseManaged())
    device_status_provider_.reset(new DevicePolicyStatusProvider(connector));

  const chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  if (user_manager->IsLoggedInAsPublicAccount()) {
    policy::DeviceLocalAccountPolicyService* local_account_service =
        connector->GetDeviceLocalAccountPolicyService();
    if (local_account_service) {
      user_status_provider_.reset(
          new DeviceLocalAccountPolicyStatusProvider(
              user_manager->GetLoggedInUser()->email(), local_account_service));
    }
  } else {
    policy::UserCloudPolicyManagerChromeOS* user_cloud_policy_manager =
        policy::UserCloudPolicyManagerFactoryChromeOS::GetForProfile(
            Profile::FromWebUI(web_ui()));
    if (user_cloud_policy_manager) {
      user_status_provider_.reset(
          new UserPolicyStatusProvider(user_cloud_policy_manager->core()));
    }
  }
#else
  policy::UserCloudPolicyManager* user_cloud_policy_manager =
      policy::UserCloudPolicyManagerFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (user_cloud_policy_manager) {
    user_status_provider_.reset(
        new UserPolicyStatusProvider(user_cloud_policy_manager->core()));
  }
#endif

  if (!user_status_provider_.get())
    user_status_provider_.reset(new CloudPolicyStatusProvider());
  if (!device_status_provider_.get())
    device_status_provider_.reset(new CloudPolicyStatusProvider());

  base::Closure update_callback(base::Bind(&PolicyUIHandler::SendStatus,
                                           base::Unretained(this)));
  user_status_provider_->SetStatusChangeCallback(update_callback);
  device_status_provider_->SetStatusChangeCallback(update_callback);
  GetPolicyService()->AddObserver(policy::POLICY_DOMAIN_CHROME, this);

  web_ui()->RegisterMessageCallback(
      "initialized",
      base::Bind(&PolicyUIHandler::HandleInitialized, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "reloadPolicies",
      base::Bind(&PolicyUIHandler::HandleReloadPolicies,
                 base::Unretained(this)));
}

void PolicyUIHandler::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                      const policy::PolicyMap& previous,
                                      const policy::PolicyMap& current) {
  DCHECK_EQ(policy::POLICY_DOMAIN_CHROME, ns.domain);
  DCHECK(ns.component_id.empty());
  SendPolicyValues();
}

void PolicyUIHandler::SendPolicyNames() const {
  base::DictionaryValue names;
  const policy::PolicyDefinitionList* list =
      policy::GetChromePolicyDefinitionList();
  for (const policy::PolicyDefinitionList::Entry* entry = list->begin;
       entry != list->end; ++entry) {
    names.SetBoolean(entry->name, true);
  }
  web_ui()->CallJavascriptFunction("policy.Page.setPolicyNames", names);
}

void PolicyUIHandler::SendPolicyValues() const {
  // Make a copy that can be modified, since some policy values are modified
  // before being displayed.
  policy::PolicyMap map;
  map.CopyFrom(GetPolicyService()->GetPolicies(policy::PolicyNamespace(
      policy::POLICY_DOMAIN_CHROME, std::string())));

  // Get a list of all the errors in the policy values.
  const policy::ConfigurationPolicyHandlerList* handler_list =
      g_browser_process->browser_policy_connector()->GetHandlerList();
  policy::PolicyErrorMap errors;
  handler_list->ApplyPolicySettings(map, NULL, &errors);

  // Convert dictionary values to strings for display.
  handler_list->PrepareForDisplaying(&map);

  base::DictionaryValue values;
  for (policy::PolicyMap::const_iterator entry = map.begin();
      entry != map.end(); ++entry) {
    base::DictionaryValue* value = new base::DictionaryValue;
    value->Set("value", entry->second.value->DeepCopy());
    if (entry->second.scope == policy::POLICY_SCOPE_USER)
      value->SetString("scope", "user");
    else
      value->SetString("scope", "machine");
    if (entry->second.level == policy::POLICY_LEVEL_RECOMMENDED)
      value->SetString("level", "recommended");
    else
      value->SetString("level", "mandatory");
    string16 error = errors.GetErrors(entry->first);
    if (!error.empty())
      value->SetString("error", error);
    values.Set(entry->first, value);
  }

  web_ui()->CallJavascriptFunction("policy.Page.setPolicyValues", values);
}

void PolicyUIHandler::SendStatus() const {
  scoped_ptr<base::DictionaryValue> device_status(new base::DictionaryValue);
  device_status_provider_->GetStatus(device_status.get());
  if (!device_domain_.empty())
    device_status->SetString("domain", device_domain_);
  scoped_ptr<base::DictionaryValue> user_status(new base::DictionaryValue);
  user_status_provider_->GetStatus(user_status.get());
  std::string username;
  user_status->GetString("username", &username);
  if (!username.empty())
    user_status->SetString("domain", gaia::ExtractDomainName(username));

  base::DictionaryValue status;
  if (!device_status->empty())
    status.Set("device", device_status.release());
  if (!user_status->empty())
    status.Set("user", user_status.release());

  web_ui()->CallJavascriptFunction("policy.Page.setStatus", status);
}

void PolicyUIHandler::HandleInitialized(const base::ListValue* args) {
  SendPolicyNames();
  SendPolicyValues();
  SendStatus();
}

void PolicyUIHandler::HandleReloadPolicies(const base::ListValue* args) {
  GetPolicyService()->RefreshPolicies(
      base::Bind(&PolicyUIHandler::OnRefreshPoliciesDone,
                 weak_factory_.GetWeakPtr()));
}

void PolicyUIHandler::OnRefreshPoliciesDone() const {
  web_ui()->CallJavascriptFunction("policy.Page.reloadPoliciesDone");
}

policy::PolicyService* PolicyUIHandler::GetPolicyService() const {
  return policy::ProfilePolicyConnectorFactory::GetForProfile(
      Profile::FromWebUI(web_ui()))->policy_service();
}

PolicyUI::PolicyUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new PolicyUIHandler);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                CreatePolicyUIHTMLSource());
}

PolicyUI::~PolicyUI() {
}
