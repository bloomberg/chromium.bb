// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_url_util.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/translate/translate_manager.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"

namespace {

// Used in all translate URLs to specify API Key.
const char kApiKeyName[] = "key";

// Used in kTranslateScriptURL and kLanguageListFetchURL to specify the
// application locale.
const char kHostLocaleQueryName[] = "hl";

}  // namespace

namespace TranslateURLUtil {

GURL AddApiKeyToUrl(const GURL& url) {
  return net::AppendQueryParameter(url, kApiKeyName, google_apis::GetAPIKey());
}

GURL AddHostLocaleToUrl(const GURL& url) {
  return net::AppendQueryParameter(
      url,
      kHostLocaleQueryName,
      TranslateManager::GetLanguageCode(
          g_browser_process->GetApplicationLocale()));
}

}  // namespace TranslateURLUtil
