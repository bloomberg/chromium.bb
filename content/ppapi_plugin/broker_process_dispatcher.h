// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_BROKER_PROCESS_DISPATCHER_H_
#define CONTENT_PPAPI_PLUGIN_BROKER_PROCESS_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/c/ppp.h"
#include "ppapi/proxy/broker_dispatcher.h"
#include "ppapi/shared_impl/ppp_flash_browser_operations_shared.h"

// Wrapper around a BrokerDispatcher that provides the necessary integration
// for plugin process management. This class is to avoid direct dependencies
// from the PPAPI proxy on the Chrome multiprocess infrastructure.
class BrokerProcessDispatcher
    : public ppapi::proxy::BrokerSideDispatcher,
      public base::SupportsWeakPtr<BrokerProcessDispatcher> {
 public:
  BrokerProcessDispatcher(PP_GetInterface_Func get_plugin_interface,
                          PP_ConnectInstance_Func connect_instance);
  virtual ~BrokerProcessDispatcher();

  // IPC::Channel::Listener overrides.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  void OnGetPermissionSettingsCompleted(
      uint32 request_id,
      bool success,
      PP_Flash_BrowserOperations_Permission default_permission,
      const ppapi::FlashSiteSettings& sites);

 private:
  void OnMsgClearSiteData(const FilePath& plugin_data_path,
                          const std::string& site,
                          uint64 flags,
                          uint64 max_age);
  void OnMsgDeauthorizeContentLicenses(uint32 request_id,
                                       const FilePath& plugin_data_path);
  void OnMsgGetPermissionSettings(
      uint32 request_id,
      const FilePath& plugin_data_path,
      PP_Flash_BrowserOperations_SettingType setting_type);
  void OnMsgSetDefaultPermission(
      uint32 request_id,
      const FilePath& plugin_data_path,
      PP_Flash_BrowserOperations_SettingType setting_type,
      PP_Flash_BrowserOperations_Permission permission,
      bool clear_site_specific);
  void OnMsgSetSitePermission(
      uint32 request_id,
      const FilePath& plugin_data_path,
      PP_Flash_BrowserOperations_SettingType setting_type,
      const ppapi::FlashSiteSettings& sites);

  // Requests that the plugin clear data, returning true on success.
  bool ClearSiteData(const FilePath& plugin_data_path,
                     const std::string& site,
                     uint64 flags,
                     uint64 max_age);
  bool DeauthorizeContentLicenses(const FilePath& plugin_data_path);
  bool SetDefaultPermission(const FilePath& plugin_data_path,
                            PP_Flash_BrowserOperations_SettingType setting_type,
                            PP_Flash_BrowserOperations_Permission permission,
                            bool clear_site_specific);
  bool SetSitePermission(const FilePath& plugin_data_path,
                         PP_Flash_BrowserOperations_SettingType setting_type,
                         const ppapi::FlashSiteSettings& sites);

  PP_GetInterface_Func get_plugin_interface_;

  const PPP_Flash_BrowserOperations_1_1* flash_browser_operations_1_1_;
  const PPP_Flash_BrowserOperations_1_0* flash_browser_operations_1_0_;

  DISALLOW_COPY_AND_ASSIGN(BrokerProcessDispatcher);
};

#endif  // CONTENT_PPAPI_PLUGIN_BROKER_PROCESS_DISPATCHER_H_
