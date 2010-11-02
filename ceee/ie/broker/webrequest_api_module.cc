// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The part of WebRequest implementation that needs to reside on the broker
// side.

#include "ceee/ie/broker/webrequest_api_module.h"

#include "ceee/ie/broker/api_dispatcher.h"
#include "ceee/ie/broker/api_module_util.h"
#include "chrome/browser/extensions/extension_webrequest_api_constants.h"

namespace ext = extension_webrequest_api_constants;

namespace webrequest_api {

void RegisterInvocations(ApiDispatcher* dispatcher) {
  // Register the permanent event handler.
  dispatcher->RegisterPermanentEventHandler(
      ext::kOnBeforeRequest,
      api_module_util::ConvertTabIdInDictionary<ext::kTabIdKey>);
}

}  // namespace webrequest_api
