// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/chrome_update_client_config.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/version.h"
#include "chrome/browser/component_updater/component_patcher_operation_out_of_process.h"
#include "chrome/browser/component_updater/component_updater_utils.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "chrome/common/channel_info.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_prefs.h"

namespace extensions {

namespace {

class ExtensionActivityDataService final
    : public update_client::ActivityDataService {
 public:
  explicit ExtensionActivityDataService(ExtensionPrefs* extension_prefs);
  ~ExtensionActivityDataService() override {}

  // update_client::ActivityDataService:
  bool GetActiveBit(const std::string& id) const override;
  int GetDaysSinceLastActive(const std::string& id) const override;
  int GetDaysSinceLastRollCall(const std::string& id) const override;
  void ClearActiveBit(const std::string& id) override;

 private:
  // This member is not owned by this class, it's owned by a profile keyed
  // service.
  ExtensionPrefs* extension_prefs_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActivityDataService);
};

// Calculates the value to use for the ping days parameter.
int CalculatePingDays(const base::Time& last_ping_day) {
  return last_ping_day.is_null()
             ? update_client::kDaysFirstTime
             : std::max((base::Time::Now() - last_ping_day).InDays(), 0);
}

ExtensionActivityDataService::ExtensionActivityDataService(
    ExtensionPrefs* extension_prefs)
    : extension_prefs_(extension_prefs) {
  DCHECK(extension_prefs_);
}

bool ExtensionActivityDataService::GetActiveBit(const std::string& id) const {
  return extension_prefs_->GetActiveBit(id);
}

int ExtensionActivityDataService::GetDaysSinceLastActive(
    const std::string& id) const {
  return CalculatePingDays(extension_prefs_->LastActivePingDay(id));
}

int ExtensionActivityDataService::GetDaysSinceLastRollCall(
    const std::string& id) const {
  return CalculatePingDays(extension_prefs_->LastPingDay(id));
}

void ExtensionActivityDataService::ClearActiveBit(const std::string& id) {
  extension_prefs_->SetActiveBit(id, false);
}

}  // namespace

// For privacy reasons, requires encryption of the component updater
// communication with the update backend.
ChromeUpdateClientConfig::ChromeUpdateClientConfig(
    content::BrowserContext* context)
    : impl_(base::CommandLine::ForCurrentProcess(),
            content::BrowserContext::GetDefaultStoragePartition(context)
                ->GetURLRequestContext(),
            true),
      pref_service_(ExtensionPrefs::Get(context)->pref_service()),
      activity_data_service_(std::make_unique<ExtensionActivityDataService>(
          ExtensionPrefs::Get(context))) {
  DCHECK(pref_service_);
}

int ChromeUpdateClientConfig::InitialDelay() const {
  return impl_.InitialDelay();
}

int ChromeUpdateClientConfig::NextCheckDelay() const {
  return impl_.NextCheckDelay();
}

int ChromeUpdateClientConfig::OnDemandDelay() const {
  return impl_.OnDemandDelay();
}

int ChromeUpdateClientConfig::UpdateDelay() const {
  return 0;
}

std::vector<GURL> ChromeUpdateClientConfig::UpdateUrl() const {
  return impl_.UpdateUrl();
}

std::vector<GURL> ChromeUpdateClientConfig::PingUrl() const {
  return impl_.PingUrl();
}

std::string ChromeUpdateClientConfig::GetProdId() const {
  return update_client::UpdateQueryParams::GetProdIdString(
      update_client::UpdateQueryParams::ProdId::CRX);
}

base::Version ChromeUpdateClientConfig::GetBrowserVersion() const {
  return impl_.GetBrowserVersion();
}

std::string ChromeUpdateClientConfig::GetChannel() const {
  return chrome::GetChannelString();
}

std::string ChromeUpdateClientConfig::GetBrand() const {
  std::string brand;
  google_brand::GetBrand(&brand);
  return brand;
}

std::string ChromeUpdateClientConfig::GetLang() const {
  return ChromeUpdateQueryParamsDelegate::GetLang();
}

std::string ChromeUpdateClientConfig::GetOSLongName() const {
  return impl_.GetOSLongName();
}

std::string ChromeUpdateClientConfig::ExtraRequestParams() const {
  return impl_.ExtraRequestParams();
}

std::string ChromeUpdateClientConfig::GetDownloadPreference() const {
  return std::string();
}

net::URLRequestContextGetter* ChromeUpdateClientConfig::RequestContext() const {
  return impl_.RequestContext();
}

scoped_refptr<update_client::OutOfProcessPatcher>
ChromeUpdateClientConfig::CreateOutOfProcessPatcher() const {
  return base::MakeRefCounted<component_updater::ChromeOutOfProcessPatcher>();
}

bool ChromeUpdateClientConfig::EnabledDeltas() const {
  return impl_.EnabledDeltas();
}

bool ChromeUpdateClientConfig::EnabledComponentUpdates() const {
  return impl_.EnabledComponentUpdates();
}

bool ChromeUpdateClientConfig::EnabledBackgroundDownloader() const {
  return impl_.EnabledBackgroundDownloader();
}

bool ChromeUpdateClientConfig::EnabledCupSigning() const {
  return impl_.EnabledCupSigning();
}

PrefService* ChromeUpdateClientConfig::GetPrefService() const {
  return pref_service_;
}

update_client::ActivityDataService*
ChromeUpdateClientConfig::GetActivityDataService() const {
  return activity_data_service_.get();
}

bool ChromeUpdateClientConfig::IsPerUserInstall() const {
  return component_updater::IsPerUserInstall();
}

std::vector<uint8_t> ChromeUpdateClientConfig::GetRunActionKeyHash() const {
  return impl_.GetRunActionKeyHash();
}

ChromeUpdateClientConfig::~ChromeUpdateClientConfig() {}

}  // namespace extensions
