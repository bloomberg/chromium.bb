// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_ui_handler.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/policy_resources.h"
#include "chrome/grit/policy_resources_map.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "extensions/features/features.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_active_directory_policy_manager.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/settings/install_attributes.h"
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#endif

namespace em = enterprise_management;

namespace {

// Strings that map from PolicySource enum to i18n string keys and their IDs.
// Their order has to follow the order of the policy::PolicySource enum.
const PolicyStringMap kPolicySources[policy::POLICY_SOURCE_COUNT] = {
    {"sourceEnterpriseDefault", IDS_POLICY_SOURCE_ENTERPRISE_DEFAULT},
    {"sourceCloud", IDS_POLICY_SOURCE_CLOUD},
    {"sourceActiveDirectory", IDS_POLICY_SOURCE_ACTIVE_DIRECTORY},
    {"sourcePublicSessionOverride", IDS_POLICY_SOURCE_PUBLIC_SESSION_OVERRIDE},
    {"sourcePlatform", IDS_POLICY_SOURCE_PLATFORM},
};

// Formats the association state indicated by |data|. If |data| is NULL, the
// state is considered to be UNMANAGED.
base::string16 FormatAssociationState(const em::PolicyData* data) {
  if (data) {
    switch (data->state()) {
      case em::PolicyData::ACTIVE:
        return l10n_util::GetStringUTF16(IDS_POLICY_ASSOCIATION_STATE_ACTIVE);
      case em::PolicyData::UNMANAGED:
        return l10n_util::GetStringUTF16(
            IDS_POLICY_ASSOCIATION_STATE_UNMANAGED);
      case em::PolicyData::DEPROVISIONED:
        return l10n_util::GetStringUTF16(
            IDS_POLICY_ASSOCIATION_STATE_DEPROVISIONED);
    }
    NOTREACHED() << "Unknown state " << data->state();
  }

  // Default to UNMANAGED for the case of missing policy or bad state enum.
  return l10n_util::GetStringUTF16(IDS_POLICY_ASSOCIATION_STATE_UNMANAGED);
}

void GetStatusFromCore(const policy::CloudPolicyCore* core,
                       base::DictionaryValue* dict) {
  const policy::CloudPolicyStore* store = core->store();
  const policy::CloudPolicyClient* client = core->client();
  const policy::CloudPolicyRefreshScheduler* refresh_scheduler =
        core->refresh_scheduler();

  // CloudPolicyStore errors take precedence to show in the status message.
  // Other errors (such as transient policy fetching problems) get displayed
  // only if CloudPolicyStore is in STATUS_OK.
  base::string16 status =
      policy::FormatStoreStatus(store->status(), store->validation_status());
  if (store->status() == policy::CloudPolicyStore::STATUS_OK) {
    if (client && client->status() != policy::DM_STATUS_SUCCESS)
      status = policy::FormatDeviceManagementStatus(client->status());
    else if (!store->is_managed())
      status = FormatAssociationState(store->policy());
  }

  const em::PolicyData* policy = store->policy();
  std::string client_id = policy ? policy->device_id() : std::string();
  std::string username = policy ? policy->username() : std::string();

  if (policy && policy->has_annotated_asset_id())
    dict->SetString("assetId", policy->annotated_asset_id());
  if (policy && policy->has_annotated_location())
    dict->SetString("location", policy->annotated_location());
  if (policy && policy->has_directory_api_id())
    dict->SetString("directoryApiId", policy->directory_api_id());

  base::TimeDelta refresh_interval =
      base::TimeDelta::FromMilliseconds(refresh_scheduler ?
          refresh_scheduler->GetActualRefreshDelay() :
          policy::CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
  base::Time last_refresh_time = refresh_scheduler ?
      refresh_scheduler->last_refresh() : base::Time();

  bool no_error = store->status() == policy::CloudPolicyStore::STATUS_OK &&
                  client && client->status() == policy::DM_STATUS_SUCCESS;
  dict->SetBoolean("error", !no_error);
  dict->SetString("status", status);
  dict->SetString("clientId", client_id);
  dict->SetString("username", username);
  dict->SetString("refreshInterval",
                  ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                         ui::TimeFormat::LENGTH_SHORT,
                                         refresh_interval));
  dict->SetString("timeSinceLastRefresh", last_refresh_time.is_null() ?
      l10n_util::GetStringUTF16(IDS_POLICY_NEVER_FETCHED) :
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                             ui::TimeFormat::LENGTH_SHORT,
                             base::Time::NowFromSystemTime() -
                                 last_refresh_time));
}

void ExtractDomainFromUsername(base::DictionaryValue* dict) {
  std::string username;
  dict->GetString("username", &username);
  if (!username.empty())
    dict->SetString("domain", gaia::ExtractDomainName(username));
}

// Utility function that returns a JSON serialization of the given |dict|.
std::unique_ptr<base::StringValue> DictionaryToJSONString(
    const base::DictionaryValue& dict) {
  std::string json_string;
  base::JSONWriter::WriteWithOptions(dict,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json_string);
  return base::MakeUnique<base::StringValue>(json_string);
}

// Returns a copy of |value| with some values converted to a representation that
// i18n_template.js will display in a nicer way.
std::unique_ptr<base::Value> CopyAndConvert(const base::Value* value) {
  const base::DictionaryValue* dict = NULL;
  if (value->GetAsDictionary(&dict))
    return DictionaryToJSONString(*dict);

  std::unique_ptr<base::Value> copy = value->CreateDeepCopy();
  base::ListValue* list = NULL;
  if (copy->GetAsList(&list)) {
    for (size_t i = 0; i < list->GetSize(); ++i) {
      if (list->GetDictionary(i, &dict))
        list->Set(i, DictionaryToJSONString(*dict));
    }
  }

  return copy;
}

}  // namespace

// An interface for querying the status of a policy provider.  It surfaces
// things like last fetch time or status of the backing store, but not the
// actual policies themselves.
class PolicyStatusProvider {
 public:
  PolicyStatusProvider();
  virtual ~PolicyStatusProvider();

  // Sets a callback to invoke upon status changes.
  void SetStatusChangeCallback(const base::Closure& callback);

  virtual void GetStatus(base::DictionaryValue* dict);

 protected:
  void NotifyStatusChange();

 private:
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(PolicyStatusProvider);
};

// Status provider implementation that pulls cloud policy status from a
// CloudPolicyCore instance provided at construction time. Also listens for
// changes on that CloudPolicyCore and reports them through the status change
// callback.
class CloudPolicyCoreStatusProvider
    : public PolicyStatusProvider,
      public policy::CloudPolicyStore::Observer {
 public:
  explicit CloudPolicyCoreStatusProvider(policy::CloudPolicyCore* core);
  ~CloudPolicyCoreStatusProvider() override;

  // policy::CloudPolicyStore::Observer implementation.
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

 protected:
  // Policy status is read from the CloudPolicyClient, CloudPolicyStore and
  // CloudPolicyRefreshScheduler hosted by this |core_|.
  policy::CloudPolicyCore* core_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudPolicyCoreStatusProvider);
};

// A cloud policy status provider for user policy.
class UserPolicyStatusProvider : public CloudPolicyCoreStatusProvider {
 public:
  explicit UserPolicyStatusProvider(policy::CloudPolicyCore* core);
  ~UserPolicyStatusProvider() override;

  // CloudPolicyCoreStatusProvider implementation.
  void GetStatus(base::DictionaryValue* dict) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserPolicyStatusProvider);
};

#if defined(OS_CHROMEOS)
// A cloud policy status provider for device policy.
class DevicePolicyStatusProvider : public CloudPolicyCoreStatusProvider {
 public:
  explicit DevicePolicyStatusProvider(
      policy::BrowserPolicyConnectorChromeOS* connector);
  ~DevicePolicyStatusProvider() override;

  // CloudPolicyCoreStatusProvider implementation.
  void GetStatus(base::DictionaryValue* dict) override;

 private:
  std::string domain_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyStatusProvider);
};

// A cloud policy status provider that reads policy status from the policy core
// associated with the device-local account specified by |user_id| at
// construction time. The indirection via user ID and
// DeviceLocalAccountPolicyService is necessary because the device-local account
// may go away any time behind the scenes, at which point the status message
// text will indicate CloudPolicyStore::STATUS_BAD_STATE.
class DeviceLocalAccountPolicyStatusProvider
    : public PolicyStatusProvider,
      public policy::DeviceLocalAccountPolicyService::Observer {
 public:
  DeviceLocalAccountPolicyStatusProvider(
      const std::string& user_id,
      policy::DeviceLocalAccountPolicyService* service);
  ~DeviceLocalAccountPolicyStatusProvider() override;

  // PolicyStatusProvider implementation.
  void GetStatus(base::DictionaryValue* dict) override;

  // policy::DeviceLocalAccountPolicyService::Observer implementation.
  void OnPolicyUpdated(const std::string& user_id) override;
  void OnDeviceLocalAccountsChanged() override;

 private:
  const std::string user_id_;
  policy::DeviceLocalAccountPolicyService* service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyStatusProvider);
};

// Provides status for DeviceActiveDirectoryPolicyManager.
class DeviceActiveDirectoryPolicyStatusProvider : public PolicyStatusProvider {
 public:
  explicit DeviceActiveDirectoryPolicyStatusProvider(
      policy::DeviceActiveDirectoryPolicyManager* manager);
  ~DeviceActiveDirectoryPolicyStatusProvider() override;

  // PolicyStatusProvider implementation.
  void GetStatus(base::DictionaryValue* dict) override;

 private:
  const policy::CloudPolicyStore* store_;

  DISALLOW_COPY_AND_ASSIGN(DeviceActiveDirectoryPolicyStatusProvider);
};
#endif

PolicyStatusProvider::PolicyStatusProvider() {}

PolicyStatusProvider::~PolicyStatusProvider() {}

void PolicyStatusProvider::SetStatusChangeCallback(
    const base::Closure& callback) {
  callback_ = callback;
}

void PolicyStatusProvider::GetStatus(base::DictionaryValue* dict) {}

void PolicyStatusProvider::NotifyStatusChange() {
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
    policy::BrowserPolicyConnectorChromeOS* connector)
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
    const std::string& user_id,
    policy::DeviceLocalAccountPolicyService* service)
      : user_id_(user_id),
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
      service_->GetBrokerForUser(user_id_);
  if (broker) {
    GetStatusFromCore(broker->core(), dict);
  } else {
    dict->SetBoolean("error", true);
    dict->SetString("status",
                    policy::FormatStoreStatus(
                        policy::CloudPolicyStore::STATUS_BAD_STATE,
                        policy::CloudPolicyValidatorBase::VALIDATION_OK));
    dict->SetString("username", std::string());
  }
  ExtractDomainFromUsername(dict);
  dict->SetBoolean("publicAccount", true);
}

void DeviceLocalAccountPolicyStatusProvider::OnPolicyUpdated(
    const std::string& user_id) {
  if (user_id == user_id_)
    NotifyStatusChange();
}

void DeviceLocalAccountPolicyStatusProvider::OnDeviceLocalAccountsChanged() {
  NotifyStatusChange();
}

DeviceActiveDirectoryPolicyStatusProvider::
    DeviceActiveDirectoryPolicyStatusProvider(
        policy::DeviceActiveDirectoryPolicyManager* manager)
    : store_(manager->store()) {}

DeviceActiveDirectoryPolicyStatusProvider::
    ~DeviceActiveDirectoryPolicyStatusProvider() {}

// TODO(tnagel): Provide more details and/or remove unused fields from UI.  See
// https://crbug.com/664747.
void DeviceActiveDirectoryPolicyStatusProvider::GetStatus(
    base::DictionaryValue* dict) {
  base::string16 status =
      policy::FormatStoreStatus(store_->status(), store_->validation_status());
  dict->SetString("status", status);
}

#endif  // defined(OS_CHROMEOS)

PolicyUIHandler::PolicyUIHandler()
    : weak_factory_(this) {
}

PolicyUIHandler::~PolicyUIHandler() {
  GetPolicyService()->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  GetPolicyService()->RemoveObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
  policy::SchemaRegistry* registry =
      policy::SchemaRegistryServiceFactory::GetForContext(
          Profile::FromWebUI(web_ui())->GetOriginalProfile())->registry();
  registry->RemoveObserver(this);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
      ->RemoveObserver(this);
#endif
}

void PolicyUIHandler::AddLocalizedPolicyStrings(
      content::WebUIDataSource* source,
      const PolicyStringMap* strings,
      size_t count) {
  for (size_t i = 0; i < count; ++i)
    source->AddLocalizedString(strings[i].key, strings[i].string_id);
}

void PolicyUIHandler::AddCommonLocalizedStringsToSource(
      content::WebUIDataSource* source) {
  AddLocalizedPolicyStrings(source, kPolicySources,
                            static_cast<size_t>(policy::POLICY_SOURCE_COUNT));
  source->AddLocalizedString("title", IDS_POLICY_TITLE);
  source->AddLocalizedString("headerScope", IDS_POLICY_HEADER_SCOPE);
  source->AddLocalizedString("headerLevel", IDS_POLICY_HEADER_LEVEL);
  source->AddLocalizedString("headerName", IDS_POLICY_HEADER_NAME);
  source->AddLocalizedString("headerValue", IDS_POLICY_HEADER_VALUE);
  source->AddLocalizedString("headerStatus", IDS_POLICY_HEADER_STATUS);
  source->AddLocalizedString("headerSource", IDS_POLICY_HEADER_SOURCE);
  source->AddLocalizedString("scopeUser", IDS_POLICY_SCOPE_USER);
  source->AddLocalizedString("scopeDevice", IDS_POLICY_SCOPE_DEVICE);
  source->AddLocalizedString("levelRecommended", IDS_POLICY_LEVEL_RECOMMENDED);
  source->AddLocalizedString("levelMandatory", IDS_POLICY_LEVEL_MANDATORY);
  source->AddLocalizedString("ok", IDS_POLICY_OK);
  source->AddLocalizedString("unset", IDS_POLICY_UNSET);
  source->AddLocalizedString("unknown", IDS_POLICY_UNKNOWN);
  source->AddLocalizedString("notSpecified", IDS_POLICY_NOT_SPECIFIED);
  source->SetJsonPath("strings.js");
}

void PolicyUIHandler::RegisterMessages() {
#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsEnterpriseManaged()) {
    if (connector->GetDeviceActiveDirectoryPolicyManager()) {
      device_status_provider_ =
          base::MakeUnique<DeviceActiveDirectoryPolicyStatusProvider>(
              connector->GetDeviceActiveDirectoryPolicyManager());
    } else {
      device_status_provider_ =
          base::MakeUnique<DevicePolicyStatusProvider>(connector);
    }
  }

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  if (user_manager->IsLoggedInAsPublicAccount()) {
    policy::DeviceLocalAccountPolicyService* local_account_service =
        connector->GetDeviceLocalAccountPolicyService();
    if (local_account_service) {
      user_status_provider_ =
          base::MakeUnique<DeviceLocalAccountPolicyStatusProvider>(
              user_manager->GetActiveUser()->GetAccountId().GetUserEmail(),
              local_account_service);
    }
  } else {
    policy::UserCloudPolicyManagerChromeOS* user_cloud_policy_manager =
        policy::UserCloudPolicyManagerFactoryChromeOS::GetForProfile(
            Profile::FromWebUI(web_ui()));
    if (user_cloud_policy_manager) {
      user_status_provider_ =
          base::MakeUnique<UserPolicyStatusProvider>(
              user_cloud_policy_manager->core());
    }
  }
#else
  policy::UserCloudPolicyManager* user_cloud_policy_manager =
      policy::UserCloudPolicyManagerFactory::GetForBrowserContext(
          web_ui()->GetWebContents()->GetBrowserContext());
  if (user_cloud_policy_manager) {
    user_status_provider_ = base::MakeUnique<UserPolicyStatusProvider>(
        user_cloud_policy_manager->core());
  }
#endif

  if (!user_status_provider_.get())
    user_status_provider_ = base::MakeUnique<PolicyStatusProvider>();
  if (!device_status_provider_.get())
    device_status_provider_ = base::MakeUnique<PolicyStatusProvider>();

  base::Closure update_callback(base::Bind(&PolicyUIHandler::SendStatus,
                                           base::Unretained(this)));
  user_status_provider_->SetStatusChangeCallback(update_callback);
  device_status_provider_->SetStatusChangeCallback(update_callback);
  GetPolicyService()->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
  GetPolicyService()->AddObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
      ->AddObserver(this);
#endif
  policy::SchemaRegistry* registry =
      policy::SchemaRegistryServiceFactory::GetForContext(
          Profile::FromWebUI(web_ui())->GetOriginalProfile())->registry();
  registry->AddObserver(this);

  web_ui()->RegisterMessageCallback(
      "initialized",
      base::Bind(&PolicyUIHandler::HandleInitialized, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "reloadPolicies",
      base::Bind(&PolicyUIHandler::HandleReloadPolicies,
                 base::Unretained(this)));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void PolicyUIHandler::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  SendPolicyNames();
  SendPolicyValues();
}

void PolicyUIHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  SendPolicyNames();
  SendPolicyValues();
}
#endif

void PolicyUIHandler::OnSchemaRegistryUpdated(bool has_new_schemas) {
  // Update UI when new schema is added.
  if (has_new_schemas) {
    SendPolicyNames();
    SendPolicyValues();
  }
}

void PolicyUIHandler::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                      const policy::PolicyMap& previous,
                                      const policy::PolicyMap& current) {
  SendPolicyValues();
}

void PolicyUIHandler::AddPolicyName(const std::string& name,
                                    base::DictionaryValue* names) const {
    names->SetBoolean(name, true);
}

void PolicyUIHandler::SendPolicyNames() const {
  base::DictionaryValue names;

  Profile* profile = Profile::FromWebUI(web_ui());
  policy::SchemaRegistry* registry =
      policy::SchemaRegistryServiceFactory::GetForContext(
          profile->GetOriginalProfile())->registry();
  scoped_refptr<policy::SchemaMap> schema_map = registry->schema_map();

  // Add Chrome policy names.
  base::DictionaryValue* chrome_policy_names = new base::DictionaryValue;
  policy::PolicyNamespace chrome_ns(policy::POLICY_DOMAIN_CHROME, "");
  const policy::Schema* chrome_schema = schema_map->GetSchema(chrome_ns);
  for (policy::Schema::Iterator it = chrome_schema->GetPropertiesIterator();
       !it.IsAtEnd(); it.Advance()) {
    AddPolicyName(it.key(), chrome_policy_names);
  }
  names.Set("chromePolicyNames", chrome_policy_names);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Add extension policy names.
  base::DictionaryValue* extension_policy_names = new base::DictionaryValue;

  for (const scoped_refptr<const extensions::Extension>& extension :
       extensions::ExtensionRegistry::Get(profile)->enabled_extensions()) {
    // Skip this extension if it's not an enterprise extension.
    if (!extension->manifest()->HasPath(
        extensions::manifest_keys::kStorageManagedSchema))
      continue;
    base::DictionaryValue* extension_value = new base::DictionaryValue;
    extension_value->SetString("name", extension->name());
    const policy::Schema* schema =
        schema_map->GetSchema(policy::PolicyNamespace(
            policy::POLICY_DOMAIN_EXTENSIONS, extension->id()));
    base::DictionaryValue* policy_names = new base::DictionaryValue;
    if (schema && schema->valid()) {
      // Get policy names from the extension's policy schema.
      // Store in a map, not an array, for faster lookup on JS side.
      for (policy::Schema::Iterator prop = schema->GetPropertiesIterator();
           !prop.IsAtEnd(); prop.Advance()) {
        policy_names->SetBoolean(prop.key(), true);
      }
    }
    extension_value->Set("policyNames", policy_names);
    extension_policy_names->Set(extension->id(), extension_value);
  }
  names.Set("extensionPolicyNames", extension_policy_names);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  web_ui()->CallJavascriptFunctionUnsafe("policy.Page.setPolicyNames", names);
}

void PolicyUIHandler::SendPolicyValues() const {
  base::DictionaryValue all_policies;

  // Add Chrome policy values.
  base::DictionaryValue* chrome_policies = new base::DictionaryValue;
  GetChromePolicyValues(chrome_policies);
  all_policies.Set("chromePolicies", chrome_policies);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Add extension policy values.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()));
  base::DictionaryValue* extension_values = new base::DictionaryValue;

  for (const scoped_refptr<const extensions::Extension>& extension :
       registry->enabled_extensions()) {
    // Skip this extension if it's not an enterprise extension.
    if (!extension->manifest()->HasPath(
        extensions::manifest_keys::kStorageManagedSchema))
      continue;
    base::DictionaryValue* extension_policies = new base::DictionaryValue;
    policy::PolicyNamespace policy_namespace = policy::PolicyNamespace(
        policy::POLICY_DOMAIN_EXTENSIONS, extension->id());
    policy::PolicyErrorMap empty_error_map;
    GetPolicyValues(GetPolicyService()->GetPolicies(policy_namespace),
                    &empty_error_map, extension_policies);
    extension_values->Set(extension->id(), extension_policies);
  }
  all_policies.Set("extensionPolicies", extension_values);
#endif
  web_ui()->CallJavascriptFunctionUnsafe("policy.Page.setPolicyValues",
                                         all_policies);
}

void PolicyUIHandler::GetPolicyValues(const policy::PolicyMap& map,
                                      policy::PolicyErrorMap* errors,
                                      base::DictionaryValue* values) const {
  for (const auto& entry : map) {
    std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
    value->Set("value", CopyAndConvert(entry.second.value.get()));
    if (entry.second.scope == policy::POLICY_SCOPE_USER)
      value->SetString("scope", "user");
    else
      value->SetString("scope", "machine");
    if (entry.second.level == policy::POLICY_LEVEL_RECOMMENDED)
      value->SetString("level", "recommended");
    else
      value->SetString("level", "mandatory");
    value->SetString("source", kPolicySources[entry.second.source].key);
    base::string16 error = errors->GetErrors(entry.first);
    if (!error.empty())
      value->SetString("error", error);
    values->Set(entry.first, std::move(value));
  }
}

void PolicyUIHandler::GetChromePolicyValues(
    base::DictionaryValue* values) const {
  policy::PolicyService* policy_service = GetPolicyService();
  policy::PolicyMap map;

  // Make a copy that can be modified, since some policy values are modified
  // before being displayed.
  map.CopyFrom(policy_service->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string())));

  // Get a list of all the errors in the policy values.
  const policy::ConfigurationPolicyHandlerList* handler_list =
      g_browser_process->browser_policy_connector()->GetHandlerList();
  policy::PolicyErrorMap errors;
  handler_list->ApplyPolicySettings(map, NULL, &errors);

  // Convert dictionary values to strings for display.
  handler_list->PrepareForDisplaying(&map);

  GetPolicyValues(map, &errors, values);
}

void PolicyUIHandler::SendStatus() const {
  std::unique_ptr<base::DictionaryValue> device_status(
      new base::DictionaryValue);
  device_status_provider_->GetStatus(device_status.get());
  if (!device_domain_.empty())
    device_status->SetString("domain", device_domain_);
  std::unique_ptr<base::DictionaryValue> user_status(new base::DictionaryValue);
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

  web_ui()->CallJavascriptFunctionUnsafe("policy.Page.setStatus", status);
}

void PolicyUIHandler::HandleInitialized(const base::ListValue* args) {
  SendPolicyNames();
  SendPolicyValues();
  SendStatus();
}

void PolicyUIHandler::HandleReloadPolicies(const base::ListValue* args) {
#if defined(OS_CHROMEOS)
  // Allow user to manually fetch remote commands, in case invalidation
  // service is not working properly.
  // TODO(binjin): evaluate and possibly remove this after invalidation
  // service is landed and tested. http://crbug.com/480982
  policy::CloudPolicyManager* manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceCloudPolicyManager();
  // Active Directory management has no CloudPolicyManager.
  if (manager) {
    policy::RemoteCommandsService* remote_commands_service =
        manager->core()->remote_commands_service();
    if (remote_commands_service)
      remote_commands_service->FetchRemoteCommands();
  }
#endif
  GetPolicyService()->RefreshPolicies(base::Bind(
      &PolicyUIHandler::OnRefreshPoliciesDone, weak_factory_.GetWeakPtr()));
}

void PolicyUIHandler::OnRefreshPoliciesDone() const {
  web_ui()->CallJavascriptFunctionUnsafe("policy.Page.reloadPoliciesDone");
}

policy::PolicyService* PolicyUIHandler::GetPolicyService() const {
  return policy::ProfilePolicyConnectorFactory::GetForBrowserContext(
             web_ui()->GetWebContents()->GetBrowserContext())->policy_service();
}
