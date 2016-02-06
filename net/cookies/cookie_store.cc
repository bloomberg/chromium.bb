// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_store.h"

#include "net/cookies/cookie_options.h"

namespace net {

CookieStore::CookieStore() {}

CookieStore::~CookieStore() {}

void CookieStore::DeleteAllAsync(const DeleteCallback& callback) {
  DeleteAllCreatedBetweenAsync(base::Time(), base::Time::Max(), callback);
}

void CookieStore::SetForceKeepSessionState() {
  // By default, do nothing.
}

}  // namespace net
