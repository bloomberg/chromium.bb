// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition_attribute.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stages.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"

namespace {
// Error messages.
const char kUnknownConditionAttribute[] = "Unknown matching condition: '%s'";
const char kInvalidValue[] = "Condition '%s' has an invalid value";
}

namespace helpers = extension_web_request_api_helpers;

namespace extensions {

namespace keys = declarative_webrequest_constants;

//
// WebRequestConditionAttribute
//

WebRequestConditionAttribute::WebRequestConditionAttribute() {}

WebRequestConditionAttribute::~WebRequestConditionAttribute() {}

// static
bool WebRequestConditionAttribute::IsKnownType(
    const std::string& instance_type) {
  return WebRequestConditionAttributeResourceType::IsMatchingType(
      instance_type);
}

// static
scoped_ptr<WebRequestConditionAttribute>
WebRequestConditionAttribute::Create(
    const std::string& name,
    const base::Value* value,
    std::string* error) {
  if (WebRequestConditionAttributeResourceType::IsMatchingType(name)) {
    return WebRequestConditionAttributeResourceType::Create(name, value, error);
  }

  *error = base::StringPrintf(kUnknownConditionAttribute, name.c_str());
  return scoped_ptr<WebRequestConditionAttribute>(NULL);
}


//
// WebRequestConditionAttributeResourceType
//

WebRequestConditionAttributeResourceType::
WebRequestConditionAttributeResourceType(
    const std::vector<ResourceType::Type>& types)
    : types_(types) {}

WebRequestConditionAttributeResourceType::
~WebRequestConditionAttributeResourceType() {}

// static
bool WebRequestConditionAttributeResourceType::IsMatchingType(
    const std::string& instance_type) {
  return instance_type == keys::kResourceTypeKey;
}

// static
scoped_ptr<WebRequestConditionAttribute>
WebRequestConditionAttributeResourceType::Create(
    const std::string& name,
    const base::Value* value,
    std::string* error) {
  DCHECK(IsMatchingType(name));

  const ListValue* value_as_list = 0;
  if (!value->GetAsList(&value_as_list)) {
    *error = base::StringPrintf(kInvalidValue, keys::kResourceTypeKey);
    return scoped_ptr<WebRequestConditionAttribute>(NULL);
  }

  size_t number_types = value_as_list->GetSize();

  std::vector<ResourceType::Type> passed_types(number_types);
  for (size_t i = 0; i < number_types; ++i) {
    std::string resource_type_string;
    ResourceType::Type type = ResourceType::LAST_TYPE;
    if (!value_as_list->GetString(i, &resource_type_string) ||
        !helpers::ParseResourceType(resource_type_string, &type)) {
      *error = base::StringPrintf(kInvalidValue, keys::kResourceTypeKey);
      return scoped_ptr<WebRequestConditionAttribute>(NULL);
    }
    passed_types.push_back(type);
  }

  return scoped_ptr<WebRequestConditionAttribute>(
      new WebRequestConditionAttributeResourceType(passed_types));
}

int WebRequestConditionAttributeResourceType::GetStages() const {
  return ON_BEFORE_REQUEST | ON_BEFORE_SEND_HEADERS | ON_SEND_HEADERS |
      ON_HEADERS_RECEIVED | ON_AUTH_REQUIRED | ON_BEFORE_REDIRECT |
      ON_RESPONSE_STARTED | ON_COMPLETED | ON_ERROR;
}

bool WebRequestConditionAttributeResourceType::IsFulfilled(
    net::URLRequest* request,
    RequestStages request_stage) {
  if (!(request_stage & GetStages()))
    return false;
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!info)
    return false;
  return std::find(types_.begin(), types_.end(), info->GetResourceType()) !=
      types_.end();
}

WebRequestConditionAttribute::Type
WebRequestConditionAttributeResourceType::GetType() const {
  return CONDITION_RESOURCE_TYPE;
}

}  // namespace extensions
