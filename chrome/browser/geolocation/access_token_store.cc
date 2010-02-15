// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/access_token_store.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"

namespace {
// Implementation of the geolocation access token store using chrome's prefs
// store (local_state) to persist these tokens.
// TODO(joth): Calls APIs which probably can't be used from the thread that it
// will be run on; needs prefs access bounced via UI thread.
class ChromePrefsAccessTokenStore : public AccessTokenStore {
 public:
  // AccessTokenStore
  virtual bool SetAccessToken(const GURL& url,
                              const string16& access_token) {
    DictionaryValue* access_token_dictionary =
        g_browser_process->local_state()->GetMutableDictionary(
            prefs::kGeolocationAccessToken);
    access_token_dictionary->SetWithoutPathExpansion(
        UTF8ToWide(url.spec()),
        Value::CreateStringValueFromUTF16(access_token));
    return true;
  }

  virtual bool GetAccessToken(const GURL& url, string16* access_token) {
    const DictionaryValue* access_token_dictionary =
        g_browser_process->local_state()->GetDictionary(
            prefs::kGeolocationAccessToken);
    // Careful: The returned value could be NULL if the pref has never been set.
    if (access_token_dictionary == NULL)
      return false;
    return access_token_dictionary->GetStringAsUTF16WithoutPathExpansion(
        UTF8ToWide(url.spec()), access_token);
  }
};

}  // namespace

void AccessTokenStore::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kGeolocationAccessToken);
}

AccessTokenStore* NewChromePrefsAccessTokenStore();
