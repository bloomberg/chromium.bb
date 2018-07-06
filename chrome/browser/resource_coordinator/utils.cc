// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/utils.h"

#include "base/md5.h"

namespace resource_coordinator {

std::string SerializeOriginIntoDatabaseKey(const url::Origin& origin) {
  return base::MD5String(origin.host());
}

bool URLShouldBeStoredInLocalDatabase(const GURL& url) {
  // Only store information for the HTTP(S) sites for now.
  return url.SchemeIsHTTPOrHTTPS();
}

}  // namespace resource_coordinator
