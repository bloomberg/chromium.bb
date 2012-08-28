// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition_attribute.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stage.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "content/public/browser/resource_request_info.h"
#include "net/http/http_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"

using base::DictionaryValue;
using base::ListValue;
using base::StringValue;
using base::Value;

namespace {
// Error messages.
const char kUnknownConditionAttribute[] = "Unknown matching condition: '*'";
const char kInvalidValue[] = "Condition '*' has an invalid value";
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
  return
      WebRequestConditionAttributeResourceType::IsMatchingType(instance_type) ||
      WebRequestConditionAttributeContentType::IsMatchingType(instance_type) ||
      WebRequestConditionAttributeResponseHeaders::IsMatchingType(
          instance_type);
}

// static
scoped_ptr<WebRequestConditionAttribute>
WebRequestConditionAttribute::Create(
    const std::string& name,
    const base::Value* value,
    std::string* error) {
  CHECK(value != NULL && error != NULL);
  if (WebRequestConditionAttributeResourceType::IsMatchingType(name)) {
    return WebRequestConditionAttributeResourceType::Create(name, value, error);
  } else if (WebRequestConditionAttributeContentType::IsMatchingType(name)) {
    return WebRequestConditionAttributeContentType::Create(name, value, error);
  } else if (WebRequestConditionAttributeResponseHeaders::IsMatchingType(
      name)) {
    return WebRequestConditionAttributeResponseHeaders::Create(
        name, value, error);
  }

  *error = ExtensionErrorUtils::FormatErrorMessage(kUnknownConditionAttribute,
                                                   name);
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

  const ListValue* value_as_list = NULL;
  if (!value->GetAsList(&value_as_list)) {
    *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidValue,
                                                     keys::kResourceTypeKey);
    return scoped_ptr<WebRequestConditionAttribute>(NULL);
  }

  size_t number_types = value_as_list->GetSize();

  std::vector<ResourceType::Type> passed_types(number_types);
  for (size_t i = 0; i < number_types; ++i) {
    std::string resource_type_string;
    ResourceType::Type type = ResourceType::LAST_TYPE;
    if (!value_as_list->GetString(i, &resource_type_string) ||
        !helpers::ParseResourceType(resource_type_string, &type)) {
      *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidValue,
                                                       keys::kResourceTypeKey);
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
    const WebRequestRule::RequestData& request_data) const {
  if (!(request_data.stage & GetStages()))
    return false;
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_data.request);
  if (!info)
    return false;
  return std::find(types_.begin(), types_.end(), info->GetResourceType()) !=
      types_.end();
}

WebRequestConditionAttribute::Type
WebRequestConditionAttributeResourceType::GetType() const {
  return CONDITION_RESOURCE_TYPE;
}

//
// WebRequestConditionAttributeContentType
//

WebRequestConditionAttributeContentType::
WebRequestConditionAttributeContentType(
    const std::vector<std::string>& content_types,
    bool inclusive)
    : content_types_(content_types),
      inclusive_(inclusive) {}

WebRequestConditionAttributeContentType::
~WebRequestConditionAttributeContentType() {}

// static
bool WebRequestConditionAttributeContentType::IsMatchingType(
    const std::string& instance_type) {
  return instance_type == keys::kContentTypeKey ||
      instance_type == keys::kExcludeContentTypeKey;
}

// static
scoped_ptr<WebRequestConditionAttribute>
WebRequestConditionAttributeContentType::Create(
      const std::string& name,
      const base::Value* value,
      std::string* error) {
  DCHECK(IsMatchingType(name));

  const ListValue* value_as_list = NULL;
  if (!value->GetAsList(&value_as_list)) {
    *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidValue, name);
    return scoped_ptr<WebRequestConditionAttribute>(NULL);
  }
  std::vector<std::string> content_types;
  for (ListValue::const_iterator it = value_as_list->begin();
       it != value_as_list->end(); ++it) {
    std::string content_type;
    if (!(*it)->GetAsString(&content_type)) {
      *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidValue, name);
      return scoped_ptr<WebRequestConditionAttribute>(NULL);
    }
    content_types.push_back(content_type);
  }

  return scoped_ptr<WebRequestConditionAttribute>(
      new WebRequestConditionAttributeContentType(
          content_types, name == keys::kContentTypeKey));
}

int WebRequestConditionAttributeContentType::GetStages() const {
  return ON_HEADERS_RECEIVED;
}

bool WebRequestConditionAttributeContentType::IsFulfilled(
    const WebRequestRule::RequestData& request_data) const {
  if (!(request_data.stage & GetStages()))
    return false;
  std::string content_type;
  request_data.original_response_headers->GetNormalizedHeader(
      net::HttpRequestHeaders::kContentType, &content_type);
  std::string mime_type;
  std::string charset;
  bool had_charset = false;
  net::HttpUtil::ParseContentType(
      content_type, &mime_type, &charset, &had_charset, NULL);

  if (inclusive_) {
    return std::find(content_types_.begin(), content_types_.end(),
                     mime_type) != content_types_.end();
  } else {
    return std::find(content_types_.begin(), content_types_.end(),
                     mime_type) == content_types_.end();
  }
}

WebRequestConditionAttribute::Type
WebRequestConditionAttributeContentType::GetType() const {
  return CONDITION_CONTENT_TYPE;
}

//
// WebRequestConditionAttributeResponseHeaders
//

WebRequestConditionAttributeResponseHeaders::StringMatchTest::StringMatchTest(
    const std::string& data,
    MatchType type)
    : data_(data),
      type_(type) {}

WebRequestConditionAttributeResponseHeaders::StringMatchTest::~StringMatchTest()
{}

WebRequestConditionAttributeResponseHeaders::HeaderMatchTest::HeaderMatchTest(
    ScopedVector<const StringMatchTest>* name,
    ScopedVector<const StringMatchTest>* value)
    : name_(name->Pass()),
      value_(value->Pass()) {}

WebRequestConditionAttributeResponseHeaders::HeaderMatchTest::~HeaderMatchTest()
{}

WebRequestConditionAttributeResponseHeaders::
WebRequestConditionAttributeResponseHeaders(
    bool positive_test, ScopedVector<const HeaderMatchTest>* tests)
    : tests_(tests->Pass()),
      positive_test_(positive_test) {}

WebRequestConditionAttributeResponseHeaders::
~WebRequestConditionAttributeResponseHeaders() {}

// static
bool WebRequestConditionAttributeResponseHeaders::IsMatchingType(
    const std::string& instance_type) {
  return instance_type == keys::kResponseHeadersKey ||
      instance_type == keys::kExcludeResponseHeadersKey;
}

// static
scoped_ptr<WebRequestConditionAttribute>
WebRequestConditionAttributeResponseHeaders::Create(
    const std::string& name,
    const base::Value* value,
    std::string* error) {
  DCHECK(IsMatchingType(name));

  const ListValue* value_as_list = NULL;
  if (!value->GetAsList(&value_as_list)) {
    *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidValue, name);
    return scoped_ptr<WebRequestConditionAttribute>(NULL);
  }

  ScopedVector<const HeaderMatchTest> header_tests;
  for (ListValue::const_iterator it = value_as_list->begin();
       it != value_as_list->end(); ++it) {
    const DictionaryValue* tests = NULL;
    if (!(*it)->GetAsDictionary(&tests)) {
      *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidValue, name);
      return scoped_ptr<WebRequestConditionAttribute>(NULL);
    }

    scoped_ptr<const HeaderMatchTest> header_test(CreateTests(tests, error));
    if (header_test.get() == NULL)
      return scoped_ptr<WebRequestConditionAttribute>(NULL);
    header_tests.push_back(header_test.release());
  }

  scoped_ptr<WebRequestConditionAttributeResponseHeaders> result;
  result.reset(new WebRequestConditionAttributeResponseHeaders(
      name == keys::kResponseHeadersKey, &header_tests));

  return result.PassAs<WebRequestConditionAttribute>();
}

int WebRequestConditionAttributeResponseHeaders::GetStages() const {
  return ON_HEADERS_RECEIVED;
}

bool WebRequestConditionAttributeResponseHeaders::IsFulfilled(
    const WebRequestRule::RequestData& request_data) const {
  if (!(request_data.stage & GetStages()))
    return false;

  const net::HttpResponseHeaders* headers =
      request_data.original_response_headers;
  if (headers == NULL) {
    // Each header of an empty set satisfies (the negation of) everything;
    // OTOH, there is no header to satisfy even the most permissive test.
    return !positive_test_;
  }

  // Has some header already passed some header test?
  bool header_found = false;

  for (size_t i = 0; !header_found && i < tests_.size(); ++i) {
    std::string name;
    std::string value;

    void* iter = NULL;
    while (!header_found &&
           headers->EnumerateHeaderLines(&iter, &name, &value)) {
      StringToLowerASCII(&name);  // Header names are case-insensitive.
      header_found |= tests_[i]->Matches(name, value);
    }
  }

  return (positive_test_ ? header_found : !header_found);
}

WebRequestConditionAttribute::Type
WebRequestConditionAttributeResponseHeaders::GetType() const {
  return CONDITION_REQUEST_HEADERS;
}

bool WebRequestConditionAttributeResponseHeaders::StringMatchTest::Matches(
    const std::string& str) const {
  switch (type_) {
    case kPrefix:
      return StartsWithASCII(str, data_, true /*case_sensitive*/);
    case kSuffix:
      return EndsWith(str, data_, true /*case_sensitive*/);
    case kEquals:
      return data_ == str;
    case kContains:
      return str.find(data_) != std::string::npos;
  }
  // We never get past the "switch", but the compiler worries about no return.
  NOTREACHED();
  return false;
}

bool WebRequestConditionAttributeResponseHeaders::HeaderMatchTest::Matches(
    const std::string& name,
    const std::string& value) const {
  for (size_t i = 0; i < name_.size(); ++i) {
    if (!name_[i]->Matches(name))
      return false;
  }

  for (size_t i = 0; i < value_.size(); ++i) {
    if (!value_[i]->Matches(value))
      return false;
  }

  return true;
}


// static
scoped_ptr<const WebRequestConditionAttributeResponseHeaders::HeaderMatchTest>
WebRequestConditionAttributeResponseHeaders::CreateTests(
    const DictionaryValue* tests,
    std::string* error) {
  ScopedVector<const StringMatchTest> name;
  ScopedVector<const StringMatchTest> value;

  for (DictionaryValue::key_iterator key = tests->begin_keys();
       key != tests->end_keys();
       ++key) {
    bool is_name = false;  // Is this test for header name?
    MatchType match_type;
    if (*key == keys::kNamePrefixKey) {
      is_name = true;
      match_type = kPrefix;
    } else if (*key == keys::kNameSuffixKey) {
      is_name = true;
      match_type = kSuffix;
    } else if (*key == keys::kNameContainsKey) {
      is_name = true;
      match_type = kContains;
    } else if (*key == keys::kNameEqualsKey) {
      is_name = true;
      match_type = kEquals;
    } else if (*key == keys::kValuePrefixKey) {
      match_type = kPrefix;
    } else if (*key == keys::kValueSuffixKey) {
      match_type = kSuffix;
    } else if (*key == keys::kValueContainsKey) {
      match_type = kContains;
    } else if (*key == keys::kValueEqualsKey) {
      match_type = kEquals;
    } else {
      NOTREACHED();  // JSON schema type checking should prevent this.
      *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidValue, *key);
      return scoped_ptr<const HeaderMatchTest>(NULL);
    }
    const Value* content = NULL;
    // This should not fire, we already checked that |key| is there.
    CHECK(tests->Get(*key, &content));

    switch (content->GetType()) {
      case Value::TYPE_LIST: {
        const ListValue* list = NULL;
        CHECK(content->GetAsList(&list));
        for (ListValue::const_iterator it = list->begin();
             it != list->end(); ++it) {
          ScopedVector<const StringMatchTest>* tests = is_name ? &name : &value;
          tests->push_back(CreateMatchTest(*it, is_name, match_type).release());
        }
        break;
      }
      case Value::TYPE_STRING: {
        ScopedVector<const StringMatchTest>* tests = is_name ? &name : &value;
        tests->push_back(
            CreateMatchTest(content, is_name, match_type).release());
        break;
      }
      default: {
        NOTREACHED();  // JSON schema type checking should prevent this.
        *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidValue, *key);
        return scoped_ptr<const HeaderMatchTest>(NULL);
      }
    }
  }

  return scoped_ptr<const HeaderMatchTest>(new HeaderMatchTest(&name, &value));
}

// static
scoped_ptr<const WebRequestConditionAttributeResponseHeaders::StringMatchTest>
WebRequestConditionAttributeResponseHeaders::CreateMatchTest(
    const Value* content, bool is_name_test, MatchType match_type) {
  std::string str;

  CHECK(content->GetAsString(&str));
  if (is_name_test)  // Header names are case-insensitive.
    StringToLowerASCII(&str);

  return scoped_ptr<const StringMatchTest>(
      new StringMatchTest(str, match_type));
}

}  // namespace extensions
