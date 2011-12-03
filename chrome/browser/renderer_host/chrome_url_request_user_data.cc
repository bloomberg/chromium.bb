// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_url_request_user_data.h"

namespace {

const char* const kKeyName = "chrome_url_request_user_data";

}  // namespace

ChromeURLRequestUserData::ChromeURLRequestUserData()
    : is_prerender_(false) {
}

// static
ChromeURLRequestUserData* ChromeURLRequestUserData::Get(
    const net::URLRequest* request) {
  DCHECK(request);
  return static_cast<ChromeURLRequestUserData*>(request->GetUserData(kKeyName));
}

// static
ChromeURLRequestUserData* ChromeURLRequestUserData::Create(
    net::URLRequest* request) {
  DCHECK(request);
  DCHECK(!Get(request));
  ChromeURLRequestUserData* user_data = new ChromeURLRequestUserData();
  request->SetUserData(kKeyName, user_data);
  return user_data;
}

// static
void ChromeURLRequestUserData::Delete(net::URLRequest* request) {
  DCHECK(request);
  request->SetUserData(kKeyName, NULL);
}
