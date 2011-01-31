// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webrequest_api.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_io_event_router.h"
#include "chrome/browser/extensions/extension_webrequest_api_constants.h"
#include "googleurl/src/gurl.h"

namespace keys = extension_webrequest_api_constants;

// static
ExtensionWebRequestEventRouter* ExtensionWebRequestEventRouter::GetInstance() {
  return Singleton<ExtensionWebRequestEventRouter>::get();
}

ExtensionWebRequestEventRouter::ExtensionWebRequestEventRouter() {
}

ExtensionWebRequestEventRouter::~ExtensionWebRequestEventRouter() {
}

void ExtensionWebRequestEventRouter::OnBeforeRequest(
    const ExtensionIOEventRouter* event_router, const GURL& url,
    const std::string& method) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(keys::kUrlKey, url.spec());
  dict->SetString(keys::kMethodKey, method);
  // TODO(mpcomplete): implement
  dict->SetInteger(keys::kTabIdKey, 0);
  dict->SetInteger(keys::kRequestIdKey, 0);
  dict->SetString(keys::kTypeKey, "main_frame");
  dict->SetInteger(keys::kTimeStampKey, 1);
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  event_router->DispatchEvent(keys::kOnBeforeRequest, json_args);
}
