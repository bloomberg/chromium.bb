// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Funnel of Chrome Extension Cookie Events to the Broker.

#include "ceee/ie/plugin/bho/cookie_events_funnel.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/extensions/extension_cookies_api_constants.h"

HRESULT CookieEventsFunnel::OnChanged(bool removed,
                                      const cookie_api::CookieInfo& cookie) {
  DictionaryValue change_info;
  change_info.SetBoolean(extension_cookies_api_constants::kRemovedKey,
                         removed);
  cookie_api::CookieApiResult api_result(
      cookie_api::CookieApiResult::kNoRequestId);
  bool success = api_result.CreateCookieValue(cookie);
  DCHECK(success);
  if (!success) {
    return E_FAIL;
  }
  change_info.Set(extension_cookies_api_constants::kCookieKey,
                  api_result.value()->DeepCopy());
  return SendEvent(extension_cookies_api_constants::kOnChanged, change_info);
}
