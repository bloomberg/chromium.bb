// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/application_context_impl.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "components/net_log/chrome_net_log.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "net/log/net_log_capture_mode.h"

ApplicationContextImpl::ApplicationContextImpl(
    const base::CommandLine& command_line) {
  DCHECK(!GetApplicationContext());
  SetApplicationContext(this);

  net_log_.reset(new net_log::ChromeNetLog(
      base::FilePath(), net::NetLogCaptureMode::Default(),
      command_line.GetCommandLineString(), GetChannelString()));
}

ApplicationContextImpl::~ApplicationContextImpl() {
  DCHECK_EQ(this, GetApplicationContext());
  SetApplicationContext(nullptr);
}

PrefService* ApplicationContextImpl::GetLocalState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ios::GetChromeBrowserProvider()->GetLocalState();
}

net::URLRequestContextGetter*
ApplicationContextImpl::GetSystemURLRequestContext() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ios::GetChromeBrowserProvider()->GetSystemURLRequestContext();
}

const std::string& ApplicationContextImpl::GetApplicationLocale() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!application_locale_.empty());
  return application_locale_;
}

void ApplicationContextImpl::SetApplicationLocale(const std::string& locale) {
  DCHECK(thread_checker_.CalledOnValidThread());
  application_locale_ = locale;
  translate::TranslateDownloadManager::GetInstance()->set_application_locale(
      application_locale_);
}

ios::ChromeBrowserStateManager*
ApplicationContextImpl::GetChromeBrowserStateManager() {
  return ios::GetChromeBrowserProvider()->GetChromeBrowserStateManager();
}

metrics::MetricsService* ApplicationContextImpl::GetMetricsService() {
  return ios::GetChromeBrowserProvider()->GetMetricsService();
}

policy::BrowserPolicyConnector*
ApplicationContextImpl::GetBrowserPolicyConnector() {
  return ios::GetChromeBrowserProvider()->GetBrowserPolicyConnector();
}

rappor::RapporService* ApplicationContextImpl::GetRapporService() {
  return ios::GetChromeBrowserProvider()->GetRapporService();
}

net_log::ChromeNetLog* ApplicationContextImpl::GetNetLog() {
  return net_log_.get();
}
