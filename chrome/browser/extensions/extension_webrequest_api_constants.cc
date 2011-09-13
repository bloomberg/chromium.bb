// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webrequest_api_constants.h"

namespace extension_webrequest_api_constants {

const char kChallengerKey[] = "challenger";
const char kErrorKey[] = "error";
const char kFrameIdKey[] = "frameId";
const char kFromCache[] = "fromCache";
const char kHostKey[] = "host";
const char kIpKey[] = "ip";
const char kPortKey[] = "port";
const char kMethodKey[] = "method";
const char kRedirectUrlKey[] = "redirectUrl";
const char kRequestIdKey[] = "requestId";
const char kStatusCodeKey[] = "statusCode";
const char kStatusLineKey[] = "statusLine";
const char kTabIdKey[] = "tabId";
const char kTimeStampKey[] = "timeStamp";
const char kTypeKey[] = "type";
const char kUrlKey[] = "url";
const char kRequestHeadersKey[] = "requestHeaders";
const char kResponseHeadersKey[] = "responseHeaders";
const char kHeaderNameKey[] = "name";
const char kHeaderValueKey[] = "value";
const char kIsProxyKey[] = "isProxy";
const char kSchemeKey[] = "scheme";
const char kRealmKey[] = "realm";

const char kOnBeforeRedirect[] = "experimental.webRequest.onBeforeRedirect";
const char kOnBeforeRequest[] = "experimental.webRequest.onBeforeRequest";
const char kOnBeforeSendHeaders[] =
    "experimental.webRequest.onBeforeSendHeaders";
const char kOnCompleted[] = "experimental.webRequest.onCompleted";
const char kOnErrorOccurred[] = "experimental.webRequest.onErrorOccurred";
const char kOnResponseStarted[] = "experimental.webRequest.onResponseStarted";
const char kOnSendHeaders[] = "experimental.webRequest.onSendHeaders";
const char kOnAuthRequired[] = "experimental.webRequest.onAuthRequired";


const char kInvalidRedirectUrl[] = "redirectUrl '*' is not a valid URL.";
const char kInvalidBlockingResponse[] =
    "cancel cannot be true in the presence of other keys.";
const char kInvalidRequestFilterUrl[] = "'*' is not a valid URL pattern.";

}  // namespace extension_webrequest_api_constants
