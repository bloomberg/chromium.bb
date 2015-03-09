// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/history_state_util.h"

#include "base/logging.h"
#include "url/gurl.h"

namespace web {
namespace history_state_util {

bool IsHistoryStateChangeValid(const GURL& currentUrl,
                               const GURL& toUrl) {
  // These two checks are very important to the security of the page. We cannot
  // allow the page to change the state to an invalid URL.
  CHECK(currentUrl.is_valid());
  CHECK(toUrl.is_valid());

  return toUrl.GetOrigin() == currentUrl.GetOrigin();
}

GURL GetHistoryStateChangeUrl(const GURL& currentUrl,
                              const GURL& baseUrl,
                              const std::string& destination) {
  if (!baseUrl.is_valid())
    return GURL();
  GURL toUrl = baseUrl.Resolve(destination);

  if (!toUrl.is_valid() || !IsHistoryStateChangeValid(currentUrl, toUrl))
    return GURL();

  return toUrl;
}

}  // namespace history_state_util
}  // namespace web
