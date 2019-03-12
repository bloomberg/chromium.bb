// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_HANDLER_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/policy/core/common/policy_service.h"
#include "extensions/browser/extension_registry_observer.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if defined(OS_CHROMEOS)
// Constants defining the IDs for the localized strings sent to the page as
// load time data.
extern const char kManagementLogUploadEnabled[];
extern const char kManagementReportActivityTimes[];
extern const char kManagementReportHardwareStatus[];
extern const char kManagementReportNetworkInterfaces[];
extern const char kManagementReportUsers[];
extern const char kManagementPrinting[];
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
extern const char kCloudReportingExtensionId[];
extern const char kOnPremReportingExtensionStableId[];
extern const char kOnPremReportingExtensionBetaId[];

extern const char kManagementExtensionReportMachineName[];
extern const char kManagementExtensionReportMachineNameAddress[];
extern const char kManagementExtensionReportUsername[];
extern const char kManagementExtensionReportVersion[];
extern const char kManagementExtensionReportExtensionsPlugin[];
extern const char kManagementExtensionReportSafeBrowsingWarnings[];
extern const char kManagementExtensionReportPerfCrash[];
extern const char kManagementExtensionReportUserBrowsingData[];

extern const char kPolicyKeyReportMachineIdData[];
extern const char kPolicyKeyReportUserIdData[];
extern const char kPolicyKeyReportVersionData[];
extern const char kPolicyKeyReportPolicyData[];
extern const char kPolicyKeyReportExtensionsData[];
extern const char kPolicyKeyReportSafeBrowsingData[];
extern const char kPolicyKeyReportSystemTelemetryData[];
extern const char kPolicyKeyReportUserBrowsingData[];

extern const char kReportingTypeDevice[];
extern const char kReportingTypeExtensions[];
extern const char kReportingTypeSecurity[];
extern const char kReportingTypeUser[];
extern const char kReportingTypeUserActivity[];

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace base {
class ListValue;
}  // namespace base

namespace extensions {
class Extension;
}  // namespace extensions

namespace policy {
class PolicyService;
}  // namespace policy

class Profile;

// The JavaScript message handler for the chrome://management page.
// TODO(ydago): Increase test coverage of this class
#if BUILDFLAG(ENABLE_EXTENSIONS)
class ManagementUIHandler : public content::WebUIMessageHandler,
                            public extensions::ExtensionRegistryObserver,
                            public policy::PolicyService::Observer {
#else
class ManagementUIHandler : public content::WebUIMessageHandler {
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
 public:
  ManagementUIHandler();
  ~ManagementUIHandler() override;

  void InitializeManagementContextualStrings(
      Profile* profile,
      content::WebUIDataSource* web_data_source);
  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void OnJavascriptDisallowed() override;

 protected:
  void AddExtensionReportingInfo(base::Value* report_sources);

  virtual const policy::PolicyService* GetPolicyService() const;
  virtual const extensions::Extension* GetEnabledExtension(
      const std::string& extensionId) const;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

 private:
  std::unique_ptr<base::DictionaryValue>
  GetDataManagementContextualSourceUpdate(Profile* profile) const;

  base::string16 GetEnterpriseManagementStatusString();

  void HandleGetDeviceManagementStatus(const base::ListValue* args);

#if defined(OS_CHROMEOS)
  void HandleGetDeviceReportingInfo(const base::ListValue* args);
#endif  // defined(OS_CHROMEOS)

  void HandleGetExtensions(const base::ListValue* args);

#if defined(OS_CHROMEOS)
  void HandleGetLocalTrustRootsInfo(const base::ListValue* args);
#endif  // defined(OS_CHROMEOS)

  void HandleInitBrowserReportingInfo(const base::ListValue* args);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void NotifyBrowserReportingInfoUpdated();

  // extensions::ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  void OnManagedStateChanged();

  // policy::PolicyService::Observer
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  void AddObservers();
  void RemoveObservers();

  // To avoid double-removing the observers, which would cause a DCHECK()
  // failure.
  bool has_observers_ = false;
  bool managed_ = false;
  std::string web_ui_data_source_name_;

  PrefChangeRegistrar pref_registrar_;

  std::set<extensions::ExtensionId> reporting_extension_ids_;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  DISALLOW_COPY_AND_ASSIGN(ManagementUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_HANDLER_H_
