// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"

namespace extensions {
namespace declarative_webrequest_constants {

// Signals to which WebRequestRulesRegistries are registered.
const char kOnRequest[] = "declarativeWebRequest.onRequest";

// Keys of dictionaries.
const char kFromKey[] = "from";
const char kInstanceTypeKey[] = "instanceType";
const char kLowerPriorityThanKey[] = "lowerPriorityThan";
const char kNameKey[] = "name";
const char kRedirectUrlKey[] = "redirectUrl";
const char kResourceTypeKey[] = "resourceType";
const char kToKey[] = "to";
const char kUrlKey[] = "url";
const char kValueKey[] = "value";

// Values of dictionaries, in particular instance types
const char kAddResponseHeaderType[] =
    "declarativeWebRequest.AddResponseHeader";
const char kCancelRequestType[] = "declarativeWebRequest.CancelRequest";
const char kIgnoreRulesType[] = "declarativeWebRequest.IgnoreRules";
const char kRedirectRequestType[] = "declarativeWebRequest.RedirectRequest";
const char kRedirectByRegExType[] =
    "declarativeWebRequest.RedirectByRegEx";
const char kRedirectToEmptyDocumentType[] =
    "declarativeWebRequest.RedirectToEmptyDocument";
const char kRedirectToTransparentImageType[] =
    "declarativeWebRequest.RedirectToTransparentImage";
const char kRemoveRequestHeaderType[] =
    "declarativeWebRequest.RemoveRequestHeader";
const char kRemoveResponseHeaderType[] =
    "declarativeWebRequest.RemoveResponseHeader";
const char kRequestMatcherType[] = "declarativeWebRequest.RequestMatcher";
const char kSetRequestHeaderType[] = "declarativeWebRequest.SetRequestHeader";
}  // namespace declarative_webrequest_constants
}  // namespace extensions
