// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/component_updater/ios_component_updater_configurator.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/version.h"
#include "components/component_updater/configurator_impl.h"
#include "components/update_client/out_of_process_patcher.h"
#include "components/update_client/update_query_params.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/google/google_brand.h"
#include "ios/chrome/common/channel_info.h"

namespace component_updater {

namespace {

class IOSConfigurator : public update_client::Configurator {
 public:
  IOSConfigurator(const base::CommandLine* cmdline,
                  net::URLRequestContextGetter* url_request_getter);

  // update_client::Configurator overrides.
  int InitialDelay() const override;
  int NextCheckDelay() const override;
  int OnDemandDelay() const override;
  int UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  std::string GetProdId() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetBrand() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  std::string ExtraRequestParams() const override;
  std::string GetDownloadPreference() const override;
  net::URLRequestContextGetter* RequestContext() const override;
  scoped_refptr<update_client::OutOfProcessPatcher> CreateOutOfProcessPatcher()
      const override;
  bool EnabledDeltas() const override;
  bool EnabledComponentUpdates() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  bool IsPerUserInstall() const override;
  std::vector<uint8_t> GetRunActionKeyHash() const override;

 private:
  friend class base::RefCountedThreadSafe<IOSConfigurator>;

  ConfiguratorImpl configurator_impl_;

  ~IOSConfigurator() override {}
};

// Allows the component updater to use non-encrypted communication with the
// update backend. The security of the update checks is enforced using
// a custom message signing protocol and it does not depend on using HTTPS.
IOSConfigurator::IOSConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* url_request_getter)
    : configurator_impl_(cmdline, url_request_getter, false) {}

int IOSConfigurator::InitialDelay() const {
  return configurator_impl_.InitialDelay();
}

int IOSConfigurator::NextCheckDelay() const {
  return configurator_impl_.NextCheckDelay();
}

int IOSConfigurator::OnDemandDelay() const {
  return configurator_impl_.OnDemandDelay();
}

int IOSConfigurator::UpdateDelay() const {
  return configurator_impl_.UpdateDelay();
}

std::vector<GURL> IOSConfigurator::UpdateUrl() const {
  return configurator_impl_.UpdateUrl();
}

std::vector<GURL> IOSConfigurator::PingUrl() const {
  return configurator_impl_.PingUrl();
}

std::string IOSConfigurator::GetProdId() const {
  return update_client::UpdateQueryParams::GetProdIdString(
      update_client::UpdateQueryParams::ProdId::CHROME);
}

base::Version IOSConfigurator::GetBrowserVersion() const {
  return configurator_impl_.GetBrowserVersion();
}

std::string IOSConfigurator::GetChannel() const {
  return GetChannelString();
}

std::string IOSConfigurator::GetBrand() const {
  std::string brand;
  ios::google_brand::GetBrand(&brand);
  return brand;
}

std::string IOSConfigurator::GetLang() const {
  return GetApplicationContext()->GetApplicationLocale();
}

std::string IOSConfigurator::GetOSLongName() const {
  return configurator_impl_.GetOSLongName();
}

std::string IOSConfigurator::ExtraRequestParams() const {
  return configurator_impl_.ExtraRequestParams();
}

std::string IOSConfigurator::GetDownloadPreference() const {
  return configurator_impl_.GetDownloadPreference();
}

net::URLRequestContextGetter* IOSConfigurator::RequestContext() const {
  return configurator_impl_.RequestContext();
}

scoped_refptr<update_client::OutOfProcessPatcher>
IOSConfigurator::CreateOutOfProcessPatcher() const {
  return nullptr;
}

bool IOSConfigurator::EnabledDeltas() const {
  return configurator_impl_.EnabledDeltas();
}

bool IOSConfigurator::EnabledComponentUpdates() const {
  return configurator_impl_.EnabledComponentUpdates();
}

bool IOSConfigurator::EnabledBackgroundDownloader() const {
  return configurator_impl_.EnabledBackgroundDownloader();
}

bool IOSConfigurator::EnabledCupSigning() const {
  return configurator_impl_.EnabledCupSigning();
}

PrefService* IOSConfigurator::GetPrefService() const {
  return GetApplicationContext()->GetLocalState();
}

bool IOSConfigurator::IsPerUserInstall() const {
  return true;
}

std::vector<uint8_t> IOSConfigurator::GetRunActionKeyHash() const {
  return configurator_impl_.GetRunActionKeyHash();
}

}  // namespace

scoped_refptr<update_client::Configurator> MakeIOSComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* context_getter) {
  return base::MakeRefCounted<IOSConfigurator>(cmdline, context_getter);
}

}  // namespace component_updater
