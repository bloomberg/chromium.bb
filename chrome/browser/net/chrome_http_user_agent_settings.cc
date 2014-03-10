// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_http_user_agent_settings.h"

#include "base/prefs/pref_service.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_util.h"

ChromeHttpUserAgentSettings::ChromeHttpUserAgentSettings(PrefService* prefs) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  pref_accept_language_.Init(prefs::kAcceptLanguages, prefs);
  last_pref_accept_language_ = *pref_accept_language_;
  last_http_accept_language_ =
      net::HttpUtil::GenerateAcceptLanguageHeader(last_pref_accept_language_);
  pref_accept_language_.MoveToThread(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO));
}

ChromeHttpUserAgentSettings::~ChromeHttpUserAgentSettings() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
}

void ChromeHttpUserAgentSettings::CleanupOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  pref_accept_language_.Destroy();
}

std::string ChromeHttpUserAgentSettings::GetAcceptLanguage() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  std::string new_pref_accept_language = *pref_accept_language_;
  if (new_pref_accept_language != last_pref_accept_language_) {
    last_http_accept_language_ =
        net::HttpUtil::GenerateAcceptLanguageHeader(new_pref_accept_language);
    last_pref_accept_language_ = new_pref_accept_language;
  }
  return last_http_accept_language_;
}

std::string ChromeHttpUserAgentSettings::GetUserAgent() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return ::GetUserAgent();
}

