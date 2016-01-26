// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/version.h"
#include "chrome/browser/component_updater/component_patcher_operation_out_of_process.h"
#include "chrome/browser/extensions/updater/chrome_update_client_config.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "chrome/common/channel_info.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

ChromeUpdateClientConfig::ChromeUpdateClientConfig(
    content::BrowserContext* context)
    : impl_(base::CommandLine::ForCurrentProcess(),
            context->GetRequestContext()) {
}

int ChromeUpdateClientConfig::InitialDelay() const {
  return impl_.InitialDelay();
}

int ChromeUpdateClientConfig::NextCheckDelay() const {
  return impl_.NextCheckDelay();
}

int ChromeUpdateClientConfig::StepDelay() const {
  return impl_.StepDelay();
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

base::Version ChromeUpdateClientConfig::GetBrowserVersion() const {
  return impl_.GetBrowserVersion();
}

std::string ChromeUpdateClientConfig::GetChannel() const {
  return chrome::GetChannelString();
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
  return make_scoped_refptr(new component_updater::ChromeOutOfProcessPatcher);
}

bool ChromeUpdateClientConfig::DeltasEnabled() const {
  return impl_.DeltasEnabled();
}

bool ChromeUpdateClientConfig::UseBackgroundDownloader() const {
  return impl_.UseBackgroundDownloader();
}

ChromeUpdateClientConfig::~ChromeUpdateClientConfig() {}

}  // namespace extensions
