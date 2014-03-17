// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/browser_policy_connector_ios.h"

#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_loader_ios.h"
#include "net/url_request/url_request_context_getter.h"

namespace policy {

namespace {

class DeviceManagementServiceConfiguration
    : public DeviceManagementService::Configuration {
 public:
  explicit DeviceManagementServiceConfiguration(const std::string& user_agent)
      : user_agent_(user_agent) {}

  virtual ~DeviceManagementServiceConfiguration() {}

  virtual std::string GetServerUrl() OVERRIDE {
    return BrowserPolicyConnector::GetDeviceManagementUrl();
  }

  virtual std::string GetAgentParameter() OVERRIDE {
    return user_agent_;
  }

  virtual std::string GetPlatformParameter() OVERRIDE {
    std::string os_name = base::SysInfo::OperatingSystemName();
    std::string os_hardware = base::SysInfo::OperatingSystemArchitecture();
    std::string os_version("-");
    int32 os_major_version = 0;
    int32 os_minor_version = 0;
    int32 os_bugfix_version = 0;
    base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                                 &os_minor_version,
                                                 &os_bugfix_version);
    os_version = base::StringPrintf("%d.%d.%d",
                                    os_major_version,
                                    os_minor_version,
                                    os_bugfix_version);
    return base::StringPrintf(
        "%s|%s|%s", os_name.c_str(), os_hardware.c_str(), os_version.c_str());
  }

 private:
  std::string user_agent_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementServiceConfiguration);
};

}  // namespace

BrowserPolicyConnectorIOS::BrowserPolicyConnectorIOS(
    const HandlerListFactory& handler_list_factory,
    const std::string& user_agent,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : BrowserPolicyConnector(handler_list_factory),
      user_agent_(user_agent) {
  scoped_ptr<AsyncPolicyLoader> loader(
      new PolicyLoaderIOS(background_task_runner));
  scoped_ptr<ConfigurationPolicyProvider> provider(
      new AsyncPolicyProvider(GetSchemaRegistry(), loader.Pass()));
  SetPlatformPolicyProvider(provider.Pass());
}

BrowserPolicyConnectorIOS::~BrowserPolicyConnectorIOS() {}

void BrowserPolicyConnectorIOS::Init(
    PrefService* local_state,
    scoped_refptr<net::URLRequestContextGetter> request_context) {
  scoped_ptr<DeviceManagementService::Configuration> configuration(
      new DeviceManagementServiceConfiguration(user_agent_));
  scoped_ptr<DeviceManagementService> device_management_service(
      new DeviceManagementService(configuration.Pass()));

  // Delay initialization of the cloud policy requests by 5 seconds.
  const int64 kServiceInitializationStartupDelay = 5000;
  device_management_service->ScheduleInitialization(
      kServiceInitializationStartupDelay);

  BrowserPolicyConnector::Init(
      local_state, request_context, device_management_service.Pass());
}

}  // namespace policy
