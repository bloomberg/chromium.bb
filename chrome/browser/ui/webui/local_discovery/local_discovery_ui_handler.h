// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LOCAL_DISCOVERY_LOCAL_DISCOVERY_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_LOCAL_DISCOVERY_LOCAL_DISCOVERY_UI_HANDLER_H_

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/local_discovery/cloud_print_account_manager.h"
#include "chrome/browser/local_discovery/privet_confirm_api_flow.h"
#include "chrome/browser/local_discovery/privet_constants.h"
#include "chrome/browser/local_discovery/privet_device_lister.h"
#include "chrome/browser/local_discovery/privet_http.h"
#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"
#include "chrome/browser/local_discovery/service_discovery_host_client.h"
#include "chrome/common/local_discovery/service_discovery_client.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui_message_handler.h"

// TODO(noamsml): Factor out full registration flow into single class
namespace local_discovery {

// UI Handler for chrome://devices/
// It listens to local discovery notifications and passes those notifications
// into the Javascript to update the page.
class LocalDiscoveryUIHandler : public content::WebUIMessageHandler,
                                public PrivetRegisterOperation::Delegate,
                                public PrivetDeviceLister::Delegate {
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

 private:
  // Message handlers:
  // For registering a device.
  void HandleRegisterDevice(const base::ListValue* args);

  // For when the page is ready to recieve device notifications.
  void HandleStart(const base::ListValue* args);

  // For when a visibility change occurs.
  void HandleIsVisible(const base::ListValue* args);

  // For when a user choice is made.
  void HandleChooseUser(const base::ListValue* args);

  // For when a cancelation is made.
  void HandleCancelRegistration(const base::ListValue* args);

  // For when the IP address of the printer has been resolved for registration.
  void StartRegisterHTTP(
      const std::string& user,
      scoped_ptr<PrivetHTTPClient> http_client);

  // For when the confirm operation on the cloudprint server has finished
  // executing.
  void OnConfirmDone(CloudPrintBaseApiFlow::Status status);

  // For when the cloud print account list is resolved.
  void OnCloudPrintAccountsResolved(const std::vector<std::string>& accounts,
                                    const std::string& xsrf_token);

  // For when XSRF token is received for a secondary account.
  void OnXSRFTokenForSecondaryAccount(
      const GURL& automated_claim_url,
      const std::vector<std::string>& accounts,
      const std::string& xsrf_token);

  // Signal to the web interface an error has ocurred while registering.
  void SendRegisterError();

  // Singal to the web interface that registration has finished.
  void SendRegisterDone();

  // Set the visibility of the page.
  void SetIsVisible(bool visible);

  // Get the sync account email.
  std::string GetSyncAccount();

  // Get the base cloud print URL for a given device.
  const std::string& GetCloudPrintBaseUrl(const std::string& device_name);

  // Start the confirm flow for a cookie based authentication.
  void StartCookieConfirmFlow(
      int user_index,
      const std::string& xsrf_token,
      const GURL& automatic_claim_url);

  // Reset and cancel the current registration.
  void ResetCurrentRegistration();

  // The current HTTP client (used for the current operation).
  scoped_ptr<PrivetHTTPClient> current_http_client_;

  // Device currently registering.
  std::string current_register_device_;

  // The current register operation. Only one allowed at any time.
  scoped_ptr<PrivetRegisterOperation> current_register_operation_;

  // The current confirm call used during the registration flow.
  scoped_ptr<PrivetConfirmApiCallFlow> confirm_api_call_flow_;

  // The device lister used to list devices on the local network.
  scoped_ptr<PrivetDeviceLister> privet_lister_;

  // The service discovery client used listen for devices on the local network.
  scoped_refptr<ServiceDiscoveryHostClient> service_discovery_client_;

  // A factory for creating the privet HTTP Client.
  scoped_ptr<PrivetHTTPAsynchronousFactory> privet_http_factory_;

  // An object representing the resolution process for the privet_http_factory.
  scoped_ptr<PrivetHTTPAsynchronousFactory::Resolution> privet_resolution_;

  // A map of current device descriptions provided by the PrivetDeviceLister.
  std::map<std::string, DeviceDescription> device_descriptions_;

  // Whether or not the page is marked as visible.
  bool is_visible_;

  // Cloud print account manager to enumerate accounts and get XSRF token.
  scoped_ptr<CloudPrintAccountManager> cloud_print_account_manager_;

  // XSRF token.
  std::string xsrf_token_for_primary_user_;

  // Current user index (for multi-login), or kAccountIndexUseOAuth2 for sync
  // credentials.
  int current_register_user_index_;

  DISALLOW_COPY_AND_ASSIGN(LocalDiscoveryUIHandler);
};

}  // namespace local_discovery
#endif  // CHROME_BROWSER_UI_WEBUI_LOCAL_DISCOVERY_LOCAL_DISCOVERY_UI_HANDLER_H_
