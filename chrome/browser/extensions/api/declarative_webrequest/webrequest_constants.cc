// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"

namespace extensions {
namespace declarative_webrequest_constants {

// Signals to which WebRequestRulesRegistries are registered.
const char kOnRequest[] = "experimental.webRequest.onRequest";

// Keys of dictionaries.
const char kInstanceTypeKey[] = "instanceType";
const char kPortsKey[] = "ports";
const char kRedirectUrlKey[] = "redirectUrl";
const char kResourceTypeKey[] = "resourceType";
const char kSchemesKey[] = "schemes";

// Values of dictionaries, in particular instance types
const char kCancelRequestType[] = "experimental.webRequest.CancelRequest";
const char kRedirectRequestType[] = "experimental.webRequest.RedirectRequest";
const char kRequestMatcherType[] = "experimental.webRequest.RequestMatcher";

// Keys of dictionaries for URL constraints
const char kHostContainsKey[] = "hostContains";
const char kHostEqualsKey[] = "hostEquals";
const char kHostPrefixKey[] = "hostPrefix";
const char kHostSuffixKey[] = "hostSuffix";
const char kHostSuffixPathPrefixKey[] = "hostSuffixPathPrefix";
const char kPathContainsKey[] = "pathContains";
const char kPathEqualsKey[] = "pathEquals";
const char kPathPrefixKey[] = "pathPrefix";
const char kPathSuffixKey[] = "pathSuffix";
const char kQueryContainsKey[] = "queryContains";
const char kQueryEqualsKey[] = "queryEquals";
const char kQueryPrefixKey[] = "queryPrefix";
const char kQuerySuffixKey[] = "querySuffix";
const char kURLContainsKey[] = "urlContains";
const char kURLEqualsKey[] = "urlEquals";
const char kURLPrefixKey[] = "urlPrefix";
const char kURLSuffixKey[] = "urlSuffix";

}  // namespace declarative_webrequest_constants
}  // namespace extensions
