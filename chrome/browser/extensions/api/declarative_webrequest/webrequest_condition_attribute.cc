// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition_attribute.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stages.h"
#include "net/url_request/url_request.h"

namespace {
// Constants from the JavaScript API.
const char kSchemeKey[] = "scheme";

// Error messages.
const char kUnknownConditionAttribute[] = "Unknown matching condition: '%s'";
const char kInvalidValue[] = "Condition '%s' has an invalid value";
}

namespace extensions {

//
// WebRequestConditionAttribute
//

WebRequestConditionAttribute::WebRequestConditionAttribute() {}

WebRequestConditionAttribute::~WebRequestConditionAttribute() {}

// static
bool WebRequestConditionAttribute::IsKnownType(
    const std::string& instance_type) {
  return WebRequestConditionAttributeHasScheme::IsMatchingType(instance_type);
}

// static
scoped_ptr<WebRequestConditionAttribute>
WebRequestConditionAttribute::Create(
    const std::string& name,
    const base::Value* value,
    std::string* error) {
  if (WebRequestConditionAttributeHasScheme::IsMatchingType(name)) {
    return WebRequestConditionAttributeHasScheme::Create(name, value, error);
  }

  *error = base::StringPrintf(kUnknownConditionAttribute, name.c_str());
  return scoped_ptr<WebRequestConditionAttribute>(NULL);
}


//
// WebRequestConditionAttributeHasScheme
//

WebRequestConditionAttributeHasScheme::WebRequestConditionAttributeHasScheme(
    const std::string& pattern)
    : pattern_(pattern) {}

WebRequestConditionAttributeHasScheme::~WebRequestConditionAttributeHasScheme()
{}

// static
bool WebRequestConditionAttributeHasScheme::IsMatchingType(
    const std::string& instance_type) {
  return instance_type == kSchemeKey;
}

//
// WebRequestConditionAttributeHasScheme
//

// static
scoped_ptr<WebRequestConditionAttribute>
WebRequestConditionAttributeHasScheme::Create(
    const std::string& name,
    const base::Value* value,
    std::string* error) {
  DCHECK(IsMatchingType(name));

  std::string scheme;
  if (!value->GetAsString(&scheme)) {
    *error = base::StringPrintf(kInvalidValue, kSchemeKey);
    return scoped_ptr<WebRequestConditionAttribute>(NULL);
  }

  return scoped_ptr<WebRequestConditionAttribute>(
      new WebRequestConditionAttributeHasScheme(scheme));
}

int WebRequestConditionAttributeHasScheme::GetStages() const {
  return ON_BEFORE_REQUEST | ON_BEFORE_SEND_HEADERS | ON_SEND_HEADERS |
      ON_HEADERS_RECEIVED | ON_AUTH_REQUIRED | ON_BEFORE_REDIRECT |
      ON_RESPONSE_STARTED | ON_COMPLETED | ON_ERROR;
}

bool WebRequestConditionAttributeHasScheme::IsFulfilled(
    net::URLRequest* request) {
  return request->url().scheme() == pattern_;
}

WebRequestConditionAttribute::Type
WebRequestConditionAttributeHasScheme::GetType() const {
  return CONDITION_HAS_SCHEME;
}

}  // namespace extensions
