// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webrequest_api_constants.h"

namespace extension_webrequest_api_constants {

const char kErrorKey[] = "error";
const char kIpKey[] = "ip";
const char kMethodKey[] = "method";
const char kRedirectUrlKey[] = "redirectUrl";
const char kRequestIdKey[] = "requestId";
const char kStatusCodeKey[] = "statusCode";
const char kTabIdKey[] = "tabId";
const char kTimeStampKey[] = "timeStamp";
const char kTypeKey[] = "type";
const char kUrlKey[] = "url";

const char kOnBeforeRedirect[] = "experimental.webRequest.onBeforeRedirect";
const char kOnBeforeRequest[] = "experimental.webRequest.onBeforeRequest";
const char kOnBeforeSendHeaders[] =
    "experimental.webRequest.onBeforeSendHeaders";
const char kOnCompleted[] = "experimental.webRequest.onCompleted";
const char kOnErrorOccurred[] = "experimental.webRequest.onErrorOccurred";
const char kOnHeadersReceived[] = "experimental.webRequest.onHeadersReceived";
const char kOnRequestSent[] = "experimental.webRequest.onRequestSent";

const char kInvalidRedirectUrl[] = "redirectUrl '*' is not a valid URL.";

}  // namespace extension_webrequest_api_constants
