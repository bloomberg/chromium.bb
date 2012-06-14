// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WebRequest API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONSTANTS_H_
#pragma once

namespace extensions {
namespace declarative_webrequest_constants {

// Signals to which WebRequestRulesRegistries are registered.
extern const char kOnRequest[];

// Keys of dictionaries.
extern const char kFromKey[];
extern const char kInstanceTypeKey[];
extern const char kLowerPriorityThanKey[];
extern const char kNameKey[];
extern const char kRedirectUrlKey[];
extern const char kResourceTypeKey[];
extern const char kToKey[];
extern const char kUrlKey[];
extern const char kValueKey[];

// Values of dictionaries, in particular instance types
extern const char kAddResponseHeaderType[];
extern const char kCancelRequestType[];
extern const char kIgnoreRulesType[];
extern const char kRedirectByRegExType[];
extern const char kRedirectRequestType[];
extern const char kRedirectToEmptyDocumentType[];
extern const char kRedirectToTransparentImageType[];
extern const char kRemoveRequestHeaderType[];
extern const char kRemoveResponseHeaderType[];
extern const char kRequestMatcherType[];
extern const char kSetRequestHeaderType[];

}  // namespace declarative_webrequest_constants
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONSTANTS_H_
