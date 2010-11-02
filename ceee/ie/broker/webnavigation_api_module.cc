// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The part of WebNavigation implementation that needs to reside on the broker
// side.

#include "ceee/ie/broker/webnavigation_api_module.h"

#include "ceee/ie/broker/api_dispatcher.h"
#include "ceee/ie/broker/api_module_util.h"
#include "chrome/browser/extensions/extension_webnavigation_api_constants.h"

namespace ext = extension_webnavigation_api_constants;

namespace webnavigation_api {

void RegisterInvocations(ApiDispatcher* dispatcher) {
  // Register the permanent event handlers.
  dispatcher->RegisterPermanentEventHandler(
      ext::kOnBeforeNavigate,
      api_module_util::ConvertTabIdInDictionary<ext::kTabIdKey>);
  dispatcher->RegisterPermanentEventHandler(
      ext::kOnBeforeRetarget,
      api_module_util::ConvertTabIdInDictionary<ext::kSourceTabIdKey>);
  dispatcher->RegisterPermanentEventHandler(
      ext::kOnCommitted,
      api_module_util::ConvertTabIdInDictionary<ext::kTabIdKey>);
  dispatcher->RegisterPermanentEventHandler(
      ext::kOnCompleted,
      api_module_util::ConvertTabIdInDictionary<ext::kTabIdKey>);
  dispatcher->RegisterPermanentEventHandler(
      ext::kOnDOMContentLoaded,
      api_module_util::ConvertTabIdInDictionary<ext::kTabIdKey>);
  dispatcher->RegisterPermanentEventHandler(
      ext::kOnErrorOccurred,
      api_module_util::ConvertTabIdInDictionary<ext::kTabIdKey>);
}

}  // namespace webnavigation_api
