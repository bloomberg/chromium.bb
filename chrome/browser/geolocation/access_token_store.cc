// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/access_token_store.h"

#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

namespace {
class ChromePrefsAccessTokenStore : public AccessTokenStore {
 public:
  ChromePrefsAccessTokenStore();

 private:
  void LoadDictionaryStoreInUIThread(
      scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request);

  // AccessTokenStore
  virtual void DoLoadAccessTokens(
      scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request);
  virtual void SaveAccessToken(
      const GURL& server_url, const string16& access_token);

  DISALLOW_COPY_AND_ASSIGN(ChromePrefsAccessTokenStore);
};

ChromePrefsAccessTokenStore::ChromePrefsAccessTokenStore() {
}

void ChromePrefsAccessTokenStore::LoadDictionaryStoreInUIThread(
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

void ChromePrefsAccessTokenStore::DoLoadAccessTokens(
    scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, NewRunnableMethod(
      this, &ChromePrefsAccessTokenStore::LoadDictionaryStoreInUIThread,
      request));
}

void SetAccessTokenOnUIThread(const GURL& server_url, const string16& token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DictionaryValue* access_token_dictionary =
      g_browser_process->local_state()->GetMutableDictionary(
          prefs::kGeolocationAccessToken);
  access_token_dictionary->SetWithoutPathExpansion(
      server_url.spec(), Value::CreateStringValue(token));
}

void ChromePrefsAccessTokenStore::SaveAccessToken(
      const GURL& server_url, const string16& access_token) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, NewRunnableFunction(
      &SetAccessTokenOnUIThread, server_url, access_token));
}
}  // namespace

AccessTokenStore::AccessTokenStore() {
}

AccessTokenStore::~AccessTokenStore() {
}

void AccessTokenStore::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kGeolocationAccessToken);
}

AccessTokenStore::Handle AccessTokenStore::LoadAccessTokens(
    CancelableRequestConsumerBase* consumer,
    LoadAccessTokensCallbackType* callback) {
  scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request(
      new CancelableRequest<LoadAccessTokensCallbackType>(callback));
  AddRequest(request, consumer);
  DCHECK(request->handle());

  DoLoadAccessTokens(request);
  return request->handle();
}

// Creates a new access token store backed by the global chome prefs.
AccessTokenStore* NewChromePrefsAccessTokenStore() {
  return new ChromePrefsAccessTokenStore;
}
