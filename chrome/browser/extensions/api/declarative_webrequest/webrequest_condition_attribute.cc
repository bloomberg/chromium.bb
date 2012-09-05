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

using base::CaseInsensitiveCompareASCII;
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

// Manages a set of tests to be applied to name-value pairs representing
// headers. This is a helper class to header-related condition attributes.
// It contains a set of test groups. A name-value pair satisfies the whole
// set of test groups iff it passes at least one test group.
class HeaderMatcher {
 public:
  ~HeaderMatcher();

  // Creates an instance based on a list |tests| of test groups, encoded as
  // dictionaries of the type declarativeWebRequest.HeaderFilter (see
  // declarative_web_request.json).
  static scoped_ptr<const HeaderMatcher> Create(const base::ListValue* tests);

  // Does |this| match the header "|name|: |value|"?
  bool TestNameValue(const std::string& name, const std::string& value) const;

 private:
  // Represents a single string-matching test.
  class StringMatchTest {
   public:
    enum MatchType { kPrefix, kSuffix, kEquals, kContains };

    // |data| is the pattern to be matched in the position given by |type|.
    // Note that |data| must point to a StringValue object.
    static scoped_ptr<StringMatchTest> Create(const Value* data,
                                              MatchType type,
                                              bool case_sensitive);
    ~StringMatchTest();

    // Does |str| pass |this| StringMatchTest?
    bool Matches(const std::string& str) const;

   private:
    StringMatchTest(const std::string& data,
                    MatchType type,
                    bool case_sensitive);

    const std::string data_;
    const MatchType type_;
    const bool case_sensitive_;
    DISALLOW_COPY_AND_ASSIGN(StringMatchTest);
  };

  // Represents a test group -- a set of string matching tests to be applied to
  // both the header name and value.
  class HeaderMatchTest {
   public:
    ~HeaderMatchTest();

    // Gets the test group description in |tests| and creates the corresponding
    // HeaderMatchTest. On failure returns NULL.
    static scoped_ptr<const HeaderMatchTest> Create(
        const base::DictionaryValue* tests);

    // Does the header "|name|: |value|" match all tests in |this|?
    bool Matches(const std::string& name, const std::string& value) const;

   private:
    // Takes ownership of the content of both |name_match| and |value_match|.
    HeaderMatchTest(ScopedVector<const StringMatchTest>* name_match,
                    ScopedVector<const StringMatchTest>* value_match);

    // Tests to be passed by a header's name.
    const ScopedVector<const StringMatchTest> name_match_;
    // Tests to be passed by a header's value.
    const ScopedVector<const StringMatchTest> value_match_;
    DISALLOW_COPY_AND_ASSIGN(HeaderMatchTest);
  };

  explicit HeaderMatcher(ScopedVector<const HeaderMatchTest>* tests);

  const ScopedVector<const HeaderMatchTest> tests_;

  DISALLOW_COPY_AND_ASSIGN(HeaderMatcher);
};

// HeaderMatcher implementation.

HeaderMatcher::~HeaderMatcher() {}

// static
scoped_ptr<const HeaderMatcher> HeaderMatcher::Create(
    const base::ListValue* tests) {
  ScopedVector<const HeaderMatchTest> header_tests;
  for (ListValue::const_iterator it = tests->begin();
       it != tests->end(); ++it) {
    const DictionaryValue* tests = NULL;
    if (!(*it)->GetAsDictionary(&tests))
      return scoped_ptr<const HeaderMatcher>(NULL);

    scoped_ptr<const HeaderMatchTest> header_test(
        HeaderMatchTest::Create(tests));
    if (header_test.get() == NULL)
      return scoped_ptr<const HeaderMatcher>(NULL);
    header_tests.push_back(header_test.release());
  }

  return scoped_ptr<const HeaderMatcher>(new HeaderMatcher(&header_tests));
}

bool HeaderMatcher::TestNameValue(const std::string& name,
                                  const std::string& value) const {
  for (size_t i = 0; i < tests_.size(); ++i) {
    if (tests_[i]->Matches(name, value))
      return true;
  }
  return false;
}

HeaderMatcher::HeaderMatcher(ScopedVector<const HeaderMatchTest>* tests)
  : tests_(tests->Pass()) {}

// HeaderMatcher::StringMatchTest implementation.

// static
scoped_ptr<HeaderMatcher::StringMatchTest>
HeaderMatcher::StringMatchTest::Create(const Value* data,
                                       MatchType type,
                                       bool case_sensitive) {
  std::string str;
  CHECK(data->GetAsString(&str));
  return scoped_ptr<StringMatchTest>(
      new StringMatchTest(str, type, case_sensitive));
}

HeaderMatcher::StringMatchTest::~StringMatchTest() {}

bool HeaderMatcher::StringMatchTest::Matches(
    const std::string& str) const {
  switch (type_) {
    case kPrefix:
      return StartsWithASCII(str, data_, case_sensitive_);
    case kSuffix:
      return EndsWith(str, data_, case_sensitive_);
    case kEquals:
      return str.size() == data_.size() &&
             StartsWithASCII(str, data_, case_sensitive_);
    case kContains:
      if (!case_sensitive_) {
        return std::search(str.begin(), str.end(), data_.begin(), data_.end(),
                           CaseInsensitiveCompareASCII<char>()) != str.end();
      } else {
        return str.find(data_) != std::string::npos;
      }
  }
  // We never get past the "switch", but the compiler worries about no return.
  NOTREACHED();
  return false;
}

HeaderMatcher::StringMatchTest::StringMatchTest(const std::string& data,
                                                MatchType type,
                                                bool case_sensitive)
    : data_(data),
      type_(type),
      case_sensitive_(case_sensitive) {}

// HeaderMatcher::HeaderMatchTest implementation.

HeaderMatcher::HeaderMatchTest::HeaderMatchTest(
    ScopedVector<const StringMatchTest>* name_match,
    ScopedVector<const StringMatchTest>* value_match)
    : name_match_(name_match->Pass()),
      value_match_(value_match->Pass()) {}

HeaderMatcher::HeaderMatchTest::~HeaderMatchTest() {}

// static
scoped_ptr<const HeaderMatcher::HeaderMatchTest>
HeaderMatcher::HeaderMatchTest::Create(const base::DictionaryValue* tests) {
  ScopedVector<const StringMatchTest> name_match;
  ScopedVector<const StringMatchTest> value_match;

  for (DictionaryValue::key_iterator key = tests->begin_keys();
       key != tests->end_keys();
       ++key) {
    bool is_name = false;  // Is this test for header name?
    StringMatchTest::MatchType match_type;
    if (*key == keys::kNamePrefixKey) {
      is_name = true;
      match_type = StringMatchTest::kPrefix;
    } else if (*key == keys::kNameSuffixKey) {
      is_name = true;
      match_type = StringMatchTest::kSuffix;
    } else if (*key == keys::kNameContainsKey) {
      is_name = true;
      match_type = StringMatchTest::kContains;
    } else if (*key == keys::kNameEqualsKey) {
      is_name = true;
      match_type = StringMatchTest::kEquals;
    } else if (*key == keys::kValuePrefixKey) {
      match_type = StringMatchTest::kPrefix;
    } else if (*key == keys::kValueSuffixKey) {
      match_type = StringMatchTest::kSuffix;
    } else if (*key == keys::kValueContainsKey) {
      match_type = StringMatchTest::kContains;
    } else if (*key == keys::kValueEqualsKey) {
      match_type = StringMatchTest::kEquals;
    } else {
      NOTREACHED();  // JSON schema type checking should prevent this.
      return scoped_ptr<const HeaderMatchTest>(NULL);
    }
    const Value* content = NULL;
    // This should not fire, we already checked that |key| is there.
    CHECK(tests->Get(*key, &content));

    ScopedVector<const StringMatchTest>* tests =
        is_name ? &name_match : &value_match;
    switch (content->GetType()) {
      case Value::TYPE_LIST: {
        const ListValue* list = NULL;
        CHECK(content->GetAsList(&list));
        for (ListValue::const_iterator it = list->begin();
             it != list->end(); ++it) {
          tests->push_back(
              StringMatchTest::Create(*it, match_type, !is_name).release());
        }
        break;
      }
      case Value::TYPE_STRING: {
        tests->push_back(
            StringMatchTest::Create(content, match_type, !is_name).release());
        break;
      }
      default: {
        NOTREACHED();  // JSON schema type checking should prevent this.
        return scoped_ptr<const HeaderMatchTest>(NULL);
      }
    }
  }

  return scoped_ptr<const HeaderMatchTest>(
      new HeaderMatchTest(&name_match, &value_match));
}

bool HeaderMatcher::HeaderMatchTest::Matches(const std::string& name,
                                             const std::string& value) const {
  for (size_t i = 0; i < name_match_.size(); ++i) {
    if (!name_match_[i]->Matches(name))
      return false;
  }

  for (size_t i = 0; i < value_match_.size(); ++i) {
    if (!value_match_[i]->Matches(value))
      return false;
  }

  return true;
}

//
// WebRequestConditionAttributeResponseHeaders
//

WebRequestConditionAttributeResponseHeaders::
WebRequestConditionAttributeResponseHeaders(
    scoped_ptr<const HeaderMatcher>* header_matcher,
    bool positive)
    : header_matcher_(header_matcher->Pass()),
      positive_(positive) {}

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

  scoped_ptr<const HeaderMatcher> header_matcher(
      HeaderMatcher::Create(value_as_list));
  if (header_matcher.get() == NULL) {
    *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidValue, name);
    return scoped_ptr<WebRequestConditionAttribute>(NULL);
  }

  const bool positive = name == keys::kResponseHeadersKey;
  return scoped_ptr<WebRequestConditionAttribute>(
      new WebRequestConditionAttributeResponseHeaders(
          &header_matcher, positive));
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
    return !positive_;
  }

  bool passed = false;  // Did some header pass TestNameValue?
  std::string name;
  std::string value;
  void* iter = NULL;
  while (!passed && headers->EnumerateHeaderLines(&iter, &name, &value)) {
    passed |= header_matcher_->TestNameValue(name, value);
  }

  return (positive_ ? passed : !passed);
}

WebRequestConditionAttribute::Type
WebRequestConditionAttributeResponseHeaders::GetType() const {
  return CONDITION_RESPONSE_HEADERS;
}

}  // namespace extensions
