// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_HANDLER_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/buildflags/buildflags.h"

// Constants defining the IDs for the localized strings sent to the page as
// load time data.
extern const char kManagementLogUploadEnabled[];
extern const char kManagementReportActivityTimes[];
extern const char kManagementReportHardwareStatus[];
extern const char kManagementReportNetworkInterfaces[];
extern const char kManagementReportUsers[];

#if BUILDFLAG(ENABLE_EXTENSIONS)
extern const char kManagementExtensionReportMachineName[];
extern const char kManagementExtensionReportMachineNameAddress[];
extern const char kManagementExtensionReportUsername[];
extern const char kManagementExtensionReportVersion[];
extern const char kManagementExtensionReportPolicies[];
extern const char kManagementExtensionReportExtensionsPlugin[];
extern const char kManagementExtensionReportSafeBrowsingWarnings[];
extern const char kManagementExtensionReportPerfCrash[];
extern const char kManagementExtensionReportWebsiteUsageStatistics[];
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace base {
class ListValue;
}  // namespace base

// The JavaScript message handler for the chrome://management page.
class ManagementUIHandler : public content::WebUIMessageHandler {
 public:
  ManagementUIHandler();
  ~ManagementUIHandler() override;

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void HandleGetManagementTitle(const base::ListValue* args);

  void HandleGetDeviceManagementStatus(const base::ListValue* args);

  void HandleGetReportingDevice(const base::ListValue* args);

  void HandleGetReportingSecurity(const base::ListValue* args);

  void HandleGetReportingUserActivity(const base::ListValue* args);

  void HandleGetReportingWeb(const base::ListValue* args);

  void HandleGetReportingInfo(const base::ListValue* args);

  void HandleGetBrowserReportingInfo(const base::ListValue* args);

  void HandleGetExtensions(const base::ListValue* args);

  void HandleGetLocalTrustRootsInfo(const base::ListValue* args);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void AddExtensionReportingInfo(base::Value* report_sources);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  base::string16 GetEnterpriseManagementStatusString();

  DISALLOW_COPY_AND_ASSIGN(ManagementUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_HANDLER_H_
