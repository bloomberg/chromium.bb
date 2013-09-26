// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LOCAL_DISCOVERY_LOCAL_DISCOVERY_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_LOCAL_DISCOVERY_LOCAL_DISCOVERY_UI_HANDLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/local_discovery/cloud_print_printer_list.h"
#include "chrome/browser/local_discovery/privet_device_lister.h"
#include "chrome/browser/local_discovery/privet_http.h"
#include "content/public/browser/web_ui_message_handler.h"

#if defined(ENABLE_FULL_PRINTING) && !defined(OS_CHROMEOS) && \
  !defined(OS_MACOSX)
#define CLOUD_PRINT_CONNECTOR_UI_AVAILABLE
#endif

// TODO(noamsml): Factor out full registration flow into single class
namespace local_discovery {

class PrivetConfirmApiCallFlow;
class PrivetHTTPAsynchronousFactory;
class PrivetHTTPResolution;
class ServiceDiscoverySharedClient;

// UI Handler for chrome://devices/
// It listens to local discovery notifications and passes those notifications
// into the Javascript to update the page.
class LocalDiscoveryUIHandler : public content::WebUIMessageHandler,
                                public PrivetRegisterOperation::Delegate,
                                public PrivetDeviceLister::Delegate,
                                public CloudPrintPrinterList::Delegate {
 public:
  class Factory {
   public:
    virtual ~Factory() {}
    virtual LocalDiscoveryUIHandler* CreateLocalDiscoveryUIHandler() = 0;
  };

  LocalDiscoveryUIHandler();
  // This constructor should only used by tests.
  explicit LocalDiscoveryUIHandler(
      scoped_ptr<PrivetDeviceLister> privet_lister);
  virtual ~LocalDiscoveryUIHandler();

  static LocalDiscoveryUIHandler* Create();
  static void SetFactory(Factory* factory);
  static bool GetHasVisible();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // PrivetRegisterOperation::Delegate implementation.
  virtual void OnPrivetRegisterClaimToken(
      PrivetRegisterOperation* operation,
      const std::string& token,
      const GURL& url) OVERRIDE;

  virtual void OnPrivetRegisterError(
      PrivetRegisterOperation* operation,
      const std::string& action,
      PrivetRegisterOperation::FailureReason reason,
      int printer_http_code,
      const DictionaryValue* json) OVERRIDE;

  virtual void OnPrivetRegisterDone(
      PrivetRegisterOperation* operation,
      const std::string& device_id) OVERRIDE;

  // PrivetDeviceLister::Delegate implementation.
  virtual void DeviceChanged(
      bool added,
      const std::string& name,
      const DeviceDescription& description) OVERRIDE;

  virtual void DeviceRemoved(const std::string& name) OVERRIDE;

  virtual void DeviceCacheFlushed() OVERRIDE;

  // CloudPrintPrinterList::Delegate implementation.
  virtual void OnCloudPrintPrinterListReady() OVERRIDE;

  virtual void OnCloudPrintPrinterListUnavailable() OVERRIDE;

 private:
  typedef std::map<std::string, DeviceDescription> DeviceDescriptionMap;

  // Message handlers:
  // For when the page is ready to recieve device notifications.
  void HandleStart(const base::ListValue* args);

  // For when a visibility change occurs.
  void HandleIsVisible(const base::ListValue* args);

  // For when a user choice is made.
  void HandleRegisterDevice(const base::ListValue* args);

  // For when a cancelation is made.
  void HandleCancelRegistration(const base::ListValue* args);

  // For requesting the printer list.
  void HandleRequestPrinterList(const base::ListValue* args);

  // For opening URLs (relative to the Google Cloud Print base URL) in a new
  // tab.
  void HandleOpenCloudPrintURL(const base::ListValue* args);

  // For showing sync login UI.
  void HandleShowSyncUI(const base::ListValue* args);

  // For when the IP address of the printer has been resolved for registration.
  void StartRegisterHTTP(
      scoped_ptr<PrivetHTTPClient> http_client);

  // For when the confirm operation on the cloudprint server has finished
  // executing.
  void OnConfirmDone(CloudPrintBaseApiFlow::Status status);

  // Signal to the web interface an error has ocurred while registering.
  void SendRegisterError();

  // Singal to the web interface that registration has finished.
  void SendRegisterDone(const DeviceDescription& device);

  // Set the visibility of the page.
  void SetIsVisible(bool visible);

  // Get the sync account email.
  std::string GetSyncAccount();

  // Get the base cloud print URL.
  std::string GetCloudPrintBaseUrl();

  // Reset and cancel the current registration.
  void ResetCurrentRegistration();

  scoped_ptr<base::DictionaryValue> CreatePrinterInfo(
      const CloudPrintPrinterList::PrinterDetails& description);

  // Announcement hasn't been sent for a certain time after registration
  // finished. Consider it failed.
  // TODO(noamsml): Re-resolve service first.
  void OnAnnouncementTimeoutReached();

  void CheckUserLoggedIn();

#if defined(CLOUD_PRINT_CONNECTOR_UI_AVAILABLE)
  void StartCloudPrintConnector();
  void OnCloudPrintPrefsChanged();
  void ShowCloudPrintSetupDialog(const ListValue* args);
  void HandleDisableCloudPrintConnector(const ListValue* args);
  void SetupCloudPrintConnectorSection();
  void RemoveCloudPrintConnectorSection();
  void RefreshCloudPrintStatusFromService();
#endif

  // The current HTTP client (used for the current operation).
  scoped_ptr<PrivetHTTPClient> current_http_client_;

  // The current register operation. Only one allowed at any time.
  scoped_ptr<PrivetRegisterOperation> current_register_operation_;

  // The current confirm call used during the registration flow.
  scoped_ptr<PrivetConfirmApiCallFlow> confirm_api_call_flow_;

  // The device lister used to list devices on the local network.
  scoped_ptr<PrivetDeviceLister> privet_lister_;

  // The service discovery client used listen for devices on the local network.
  scoped_refptr<ServiceDiscoverySharedClient> service_discovery_client_;

  // A factory for creating the privet HTTP Client.
  scoped_ptr<PrivetHTTPAsynchronousFactory> privet_http_factory_;

  // An object representing the resolution process for the privet_http_factory.
  scoped_ptr<PrivetHTTPResolution> privet_resolution_;

  // A map of current device descriptions provided by the PrivetDeviceLister.
  DeviceDescriptionMap device_descriptions_;

  // Whether or not the page is marked as visible.
  bool is_visible_;

  // Device whose state must be updated to "registered" to complete
  // registration.
  std::string new_register_device_;

  // List of printers from cloud print.
  scoped_ptr<CloudPrintPrinterList> cloud_print_printer_list_;

  // Announcement timeout for registration.
  base::CancelableCallback<void()> registration_announce_timeout_;

#if defined(CLOUD_PRINT_CONNECTOR_UI_AVAILABLE)
  StringPrefMember cloud_print_connector_email_;
  BooleanPrefMember cloud_print_connector_enabled_;
  bool cloud_print_connector_ui_enabled_;
#endif

  DISALLOW_COPY_AND_ASSIGN(LocalDiscoveryUIHandler);
};

#undef CLOUD_PRINT_CONNECTOR_UI_AVAILABLE

}  // namespace local_discovery
#endif  // CHROME_BROWSER_UI_WEBUI_LOCAL_DISCOVERY_LOCAL_DISCOVERY_UI_HANDLER_H_
