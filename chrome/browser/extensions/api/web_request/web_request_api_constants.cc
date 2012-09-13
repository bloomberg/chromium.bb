// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_request/web_request_api_constants.h"

namespace extension_web_request_api_constants {

const char kChallengerKey[] = "challenger";
const char kErrorKey[] = "error";
const char kFrameIdKey[] = "frameId";
const char kParentFrameIdKey[] = "parentFrameId";
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
const char kRequestBodyKey[] = "requestBody";
const char kRequestBodyErrorKey[] = "error";
const char kRequestBodyFormDataKey[] = "formData";
const char kRequestBodyRawKey[] = "raw";
const char kRequestBodyRawBytesKey[] = "bytes";
const char kRequestBodyRawFileKey[] = "file";
const char kRequestHeadersKey[] = "requestHeaders";
const char kResponseHeadersKey[] = "responseHeaders";
const char kHeaderNameKey[] = "name";
const char kHeaderValueKey[] = "value";
const char kHeaderBinaryValueKey[] = "binaryValue";
const char kIsProxyKey[] = "isProxy";
const char kSchemeKey[] = "scheme";
const char kRealmKey[] = "realm";
const char kAuthCredentialsKey[] = "authCredentials";
const char kUsernameKey[] = "username";
const char kPasswordKey[] = "password";

const char kOnBeforeRedirect[] = "webRequest.onBeforeRedirect";
const char kOnBeforeRequest[] = "webRequest.onBeforeRequest";
const char kOnBeforeSendHeaders[] = "webRequest.onBeforeSendHeaders";
const char kOnCompleted[] = "webRequest.onCompleted";
const char kOnErrorOccurred[] = "webRequest.onErrorOccurred";
const char kOnHeadersReceived[] = "webRequest.onHeadersReceived";
const char kOnResponseStarted[] = "webRequest.onResponseStarted";
const char kOnSendHeaders[] = "webRequest.onSendHeaders";
const char kOnAuthRequired[] = "webRequest.onAuthRequired";


const char kInvalidRedirectUrl[] = "redirectUrl '*' is not a valid URL.";
const char kInvalidBlockingResponse[] =
    "cancel cannot be true in the presence of other keys.";
const char kInvalidRequestFilterUrl[] = "'*' is not a valid URL pattern.";
const char kBlockingPermissionRequired[] =
    "You do not have permission to use blocking webRequest listeners. "
    "Be sure to declare the webRequestBlocking permission in your "
    "manifest.";
const char kHostPermissionsRequired[] =
    "You need to request host permissions in the manifest file in order to "
    "be notified about requests from the webRequest API.";

}  // namespace extension_web_request_api_constants
