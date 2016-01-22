// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/token_binding_manager.h"

#include "android_webview/browser/aw_browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace android_webview {

using content::BrowserThread;
using net::ChannelIDService;
using net::ChannelIDStore;

namespace {

void CompletionCallback(TokenBindingManager::KeyReadyCallback callback,
                        ChannelIDService::Request* request,
                        scoped_ptr<crypto::ECPrivateKey>* key,
                        int status) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, status, base::Owned(key->release())));
}

void DeletionCompleteCallback(
    TokenBindingManager::DeletionCompleteCallback callback) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

void GetKeyImpl(const std::string& host,
                TokenBindingManager::KeyReadyCallback callback,
                scoped_refptr<net::URLRequestContextGetter> context_getter) {
  ChannelIDService* service =
      context_getter->GetURLRequestContext()->channel_id_service();
  ChannelIDService::Request* request = new ChannelIDService::Request();
  scoped_ptr<crypto::ECPrivateKey>* key =
      new scoped_ptr<crypto::ECPrivateKey>();
  // The request will own the callback if the call to service returns
  // PENDING. The request releases the ownership before calling the callback.
  net::CompletionCallback completion_callback = base::Bind(
      &CompletionCallback, callback, base::Owned(request), base::Owned(key));
  int status =
      service->GetOrCreateChannelID(host, key, completion_callback, request);
  if (status == net::ERR_IO_PENDING) {
    // The operation is pending, callback will be called async.
    return;
  }
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(completion_callback, status));
}

void DeleteKeyImpl(const std::string& host,
                   TokenBindingManager::DeletionCompleteCallback callback,
                   scoped_refptr<net::URLRequestContextGetter> context_getter,
                   bool all) {
  ChannelIDService* service =
      context_getter->GetURLRequestContext()->channel_id_service();
  ChannelIDStore* store = service->GetChannelIDStore();
  base::Closure completion_callback =
      base::Bind(&DeletionCompleteCallback, callback);
  if (all) {
    store->DeleteAll(completion_callback);
  } else {
    store->DeleteChannelID(host, completion_callback);
  }
}

}  // namespace

base::LazyInstance<TokenBindingManager>::Leaky g_lazy_instance;

TokenBindingManager* TokenBindingManager::GetInstance() {
  return g_lazy_instance.Pointer();
}

TokenBindingManager::TokenBindingManager() : enabled_(false) {}

void TokenBindingManager::GetKey(const std::string& host,
                                 KeyReadyCallback callback) {
  scoped_refptr<net::URLRequestContextGetter> context_getter =
      AwBrowserContext::GetDefault()->GetRequestContext();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&GetKeyImpl, host, callback, context_getter));
}

void TokenBindingManager::DeleteKey(const std::string& host,
                                    DeletionCompleteCallback callback) {
  scoped_refptr<net::URLRequestContextGetter> context_getter =
      AwBrowserContext::GetDefault()->GetRequestContext();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DeleteKeyImpl, host, callback, context_getter, false));
}

void TokenBindingManager::DeleteAllKeys(DeletionCompleteCallback callback) {
  scoped_refptr<net::URLRequestContextGetter> context_getter =
      AwBrowserContext::GetDefault()->GetRequestContext();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DeleteKeyImpl, "", callback, context_getter, true));
}

}  // namespace android_webview
