// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_WEBRTC_DEVTOOLS_BRIDGE_CLIENT_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_WEBRTC_DEVTOOLS_BRIDGE_CLIENT_H_

#include "chrome/browser/devtools/device/android_device_manager.h"
#include "chrome/browser/devtools/device/webrtc/devtools_bridge_instances_request.h"
#include "chrome/browser/devtools/device/webrtc/send_command_request.h"
#include "chrome/browser/local_discovery/cloud_device_list_delegate.h"
#include "components/signin/core/browser/profile_identity_provider.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "google_apis/gaia/identity_provider.h"

class Profile;
class SigninManagerBase;
class ProfileOAuth2TokenService;

namespace content {
class WebUI;
}  // namespace content

namespace local_discovery {
class GCDApiFlow;
}  // local_discovery

// Lives on the UI thread.
class DevToolsBridgeClient : protected content::WebContentsObserver,
                             private content::NotificationObserver,
                             private IdentityProvider::Observer,
                             private SendCommandRequest::Delegate,
                             private DevToolsBridgeInstancesRequest::Delegate {
 public:
  using BrowserInfo = AndroidDeviceManager::BrowserInfo;
  using DeviceInfo = AndroidDeviceManager::DeviceInfo;
  using SerialList = std::vector<std::string>;
  using BrowserInfoList = std::vector<BrowserInfo>;

  static base::WeakPtr<DevToolsBridgeClient> Create(
      Profile* profile,
      SigninManagerBase* signin_manager,
      ProfileOAuth2TokenService* token_service);

  void DeleteSelf();

  static SerialList GetDevices(base::WeakPtr<DevToolsBridgeClient> weak_ptr);
  static DeviceInfo GetDeviceInfo(base::WeakPtr<DevToolsBridgeClient> weak_ptr,
                                  const std::string& serial);
  void StartSessionIfNeeded(const std::string& socket_name);

  static DevToolsBridgeClient* FromWebContents(
      content::WebContents* web_contents);
  void RegisterMessageHandlers(content::WebUI* web_ui);

 protected:
  DevToolsBridgeClient(Profile* profile,
                       SigninManagerBase* signin_manager,
                       ProfileOAuth2TokenService* token_service);

  // Implementation of content::WebContentsObserver.
  void DocumentOnLoadCompletedInMainFrame() override;

  ~DevToolsBridgeClient() override;

  bool IsAuthenticated();

  // Overridden in tests.
  virtual scoped_ptr<local_discovery::GCDApiFlow> CreateGCDApiFlow();
  virtual void OnBrowserListUpdatedForTests() {}

  const BrowserInfoList& browsers() const { return browsers_; }
  ProfileIdentityProvider& identity_provider() { return identity_provider_; }

 private:
  void CreateBackgroundWorker();
  void UpdateBrowserList();

  void HandleSendCommand(const base::ListValue* args);

  // Implementation of IdentityProvider::Observer.
  void OnActiveAccountLogin() override;
  void OnActiveAccountLogout() override;

  // Implementation of NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Implementation of SendCommandRequest::Delegate.
  void OnCommandSucceeded(const base::DictionaryValue& response) override;
  void OnCommandFailed() override;

  // Implementation of DevToolsBridgeInstancesRequest::Delegate.
  void OnDevToolsBridgeInstancesRequestSucceeded(
      const DevToolsBridgeInstancesRequest::InstanceList& instances) override;
  void OnDevToolsBridgeInstancesRequestFailed() override;

  Profile* const profile_;
  ProfileIdentityProvider identity_provider_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<content::WebContents> background_worker_;
  scoped_ptr<local_discovery::GCDApiFlow> browser_list_request_;
  scoped_ptr<local_discovery::GCDApiFlow> send_command_request_;
  BrowserInfoList browsers_;
  bool worker_is_loaded_;
  base::WeakPtrFactory<DevToolsBridgeClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsBridgeClient);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_WEBRTC_DEVTOOLS_BRIDGE_CLIENT_H_
