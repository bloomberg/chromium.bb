// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/application_context_impl.h"

#include "base/logging.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

ApplicationContextImpl::ApplicationContextImpl() {
  DCHECK(!GetApplicationContext());
  SetApplicationContext(this);
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
