// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/cookie_store_ios_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The CookieStoreIOSClient.
net::CookieStoreIOSClient* g_client;
}  // namespace

namespace net {

void SetCookieStoreIOSClient(CookieStoreIOSClient* client) {
  g_client = client;
}

CookieStoreIOSClient* GetCookieStoreIOSClient() {
  return g_client;
}

CookieStoreIOSClient::CookieStoreIOSClient() {}

CookieStoreIOSClient::~CookieStoreIOSClient() {}

scoped_refptr<base::SequencedTaskRunner>
CookieStoreIOSClient::GetTaskRunner() const {
  return scoped_refptr<base::SequencedTaskRunner>();
}

}  // namespace net
