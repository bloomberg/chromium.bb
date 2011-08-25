// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/chrome_access_token_store.h"

#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

void ChromeAccessTokenStore::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kGeolocationAccessToken);
}

ChromeAccessTokenStore::ChromeAccessTokenStore() {
}

void ChromeAccessTokenStore::LoadDictionaryStoreInUIThread(
      scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (request->canceled())
    return;
  const DictionaryValue* token_dictionary =
          g_browser_process->local_state()->GetDictionary(
              prefs::kGeolocationAccessToken);

  AccessTokenStore::AccessTokenSet access_token_set;
  // The dictionary value could be NULL if the pref has never been set.
  if (token_dictionary != NULL) {
    for (DictionaryValue::key_iterator it = token_dictionary->begin_keys();
        it != token_dictionary->end_keys(); ++it) {
      GURL url(*it);
      if (!url.is_valid())
        continue;
      token_dictionary->GetStringWithoutPathExpansion(*it,
                                                      &access_token_set[url]);
    }
  }
  request->ForwardResultAsync(MakeTuple(access_token_set));
}

void ChromeAccessTokenStore::DoLoadAccessTokens(
    scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, NewRunnableMethod(
      this, &ChromeAccessTokenStore::LoadDictionaryStoreInUIThread,
      request));
}

void SetAccessTokenOnUIThread(const GURL& server_url, const string16& token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DictionaryPrefUpdate update(g_browser_process->local_state(),
                              prefs::kGeolocationAccessToken);
  DictionaryValue* access_token_dictionary = update.Get();
  access_token_dictionary->SetWithoutPathExpansion(
      server_url.spec(), Value::CreateStringValue(token));
}

void ChromeAccessTokenStore::SaveAccessToken(
      const GURL& server_url, const string16& access_token) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, NewRunnableFunction(
      &SetAccessTokenOnUIThread, server_url, access_token));
}
