// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_ATTRIBUTE_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_ATTRIBUTE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stage.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rule.h"
#include "chrome/common/extensions/api/events.h"
#include "webkit/glue/resource_type.h"

namespace base {
class Value;
}

namespace net {
class URLRequest;
}

namespace extensions {

// Base class for all condition attributes of the declarative Web Request API
// except for condition attribute to test URLPatterns.
class WebRequestConditionAttribute {
 public:
  enum Type {
    CONDITION_RESOURCE_TYPE,
    CONDITION_CONTENT_TYPE,
    CONDITION_REQUEST_HEADERS
  };

  WebRequestConditionAttribute();
  virtual ~WebRequestConditionAttribute();

  // Factory method that creates a WebRequestConditionAttribute for the JSON
  // dictionary {|name|: |value|} passed by the extension API. Sets |error| and
  // returns NULL if something fails.
  // The ownership of |value| remains at the caller.
  static scoped_ptr<WebRequestConditionAttribute> Create(
      const std::string& name,
      const base::Value* value,
      std::string* error);

  // Returns a bit vector representing extensions::RequestStage. The bit vector
  // contains a 1 for each request stage during which the condition attribute
  // can be tested.
  virtual int GetStages() const = 0;

  // Returns whether the condition is fulfilled for this request.
  virtual bool IsFulfilled(
      const WebRequestRule::RequestData& request_data) const = 0;

  virtual Type GetType() const = 0;

  // Returns whether condition attributes of type |instance_type| are known
  // and can be instantiated by Create().
  static bool IsKnownType(const std::string& instance_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestConditionAttribute);
};

typedef std::vector<linked_ptr<WebRequestConditionAttribute> >
    WebRequestConditionAttributes;

//
// The following are concrete condition attributes.
//

// Condition that checks whether a request is for a specific resource type.
class WebRequestConditionAttributeResourceType
    : public WebRequestConditionAttribute {
 public:
  virtual ~WebRequestConditionAttributeResourceType();

  static bool IsMatchingType(const std::string& instance_type);

  // Factory method, see WebRequestConditionAttribute::Create.
  static scoped_ptr<WebRequestConditionAttribute> Create(
      const std::string& name,
      const base::Value* value,
      std::string* error);

  // Implementation of WebRequestConditionAttribute:
  virtual int GetStages() const OVERRIDE;
  virtual bool IsFulfilled(
      const WebRequestRule::RequestData& request_data) const OVERRIDE;
  virtual Type GetType() const OVERRIDE;

 private:
  explicit WebRequestConditionAttributeResourceType(
      const std::vector<ResourceType::Type>& types);

  std::vector<ResourceType::Type> types_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestConditionAttributeResourceType);
};

// Condition that checks whether a response's Content-Type header has a
// certain MIME media type.
class WebRequestConditionAttributeContentType
    : public WebRequestConditionAttribute {
 public:
  virtual ~WebRequestConditionAttributeContentType();

  static bool IsMatchingType(const std::string& instance_type);

  // Factory method, see WebRequestConditionAttribute::Create.
  static scoped_ptr<WebRequestConditionAttribute> Create(
      const std::string& name,
      const base::Value* value,
      std::string* error);

  // Implementation of WebRequestConditionAttribute:
  virtual int GetStages() const OVERRIDE;
  virtual bool IsFulfilled(
      const WebRequestRule::RequestData& request_data) const OVERRIDE;
  virtual Type GetType() const OVERRIDE;

 private:
  explicit WebRequestConditionAttributeContentType(
      const std::vector<std::string>& include_content_types,
      bool inclusive);

  std::vector<std::string> content_types_;
  bool inclusive_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestConditionAttributeContentType);
};

// Condition that performs matches against response headers' names and values.
// In the comments below there is a distinction between when this condition is
// "satisfied" and when it is "fulfilled". See the comments at |tests_| for
// "satisfied" and the comment at |positive_test_| for "fulfilled".
class WebRequestConditionAttributeResponseHeaders
    : public WebRequestConditionAttribute {
 public:
  virtual ~WebRequestConditionAttributeResponseHeaders();

  static bool IsMatchingType(const std::string& instance_type);

  // Factory method, see WebRequestConditionAttribute::Create.
  static scoped_ptr<WebRequestConditionAttribute> Create(
      const std::string& name,
      const base::Value* value,
      std::string* error);

  // Implementation of WebRequestConditionAttribute:
  virtual int GetStages() const OVERRIDE;
  virtual bool IsFulfilled(
      const WebRequestRule::RequestData& request_data) const OVERRIDE;
  virtual Type GetType() const OVERRIDE;

 private:
  enum MatchType { kPrefix, kSuffix, kEquals, kContains };

  class StringMatchTest {
   public:
    StringMatchTest(const std::string& data, MatchType type);
    ~StringMatchTest();

    // Does |str| pass |*this| StringMatchTest?
    bool Matches(const std::string& str) const;

   private:
    const std::string data_;
    const MatchType type_;
    DISALLOW_COPY_AND_ASSIGN(StringMatchTest);
  };

  class HeaderMatchTest {
   public:
    // Takes ownership of the content of both |name| and |value|.
    HeaderMatchTest(ScopedVector<const StringMatchTest>* name,
              ScopedVector<const StringMatchTest>* value);
    ~HeaderMatchTest();
    // Does the header |name|: |value| match all tests in this header test?
    bool Matches(const std::string& name, const std::string& value) const;

   private:
    // Tests to be passed by a header's name.
    const ScopedVector<const StringMatchTest> name_;
    // Tests to be passed by a header's value.
    const ScopedVector<const StringMatchTest> value_;
    DISALLOW_COPY_AND_ASSIGN(HeaderMatchTest);
  };

  WebRequestConditionAttributeResponseHeaders(
      bool positive_test, ScopedVector<const HeaderMatchTest>* tests);

  // Gets the tests' description in |tests| and creates the corresponding
  // HeaderMatchTest. Returns NULL on failure.
  static scoped_ptr<const HeaderMatchTest> CreateTests(
      const base::DictionaryValue* tests,
      std::string* error);
  // Helper to CreateTests. Never returns NULL, except for memory failures.
  static scoped_ptr<const StringMatchTest> CreateMatchTest(const Value* content,
                                                     bool is_name_test,
                                                     MatchType match_type);

  // The condition is satisfied if there is a header and its value such that for
  // some |i| the header passes all the tests from |tests_[i]|.
  ScopedVector<const HeaderMatchTest> tests_;

  // True means that IsFulfilled() reports whether the condition is satisfied.
  // False means that it reports whether the condition is NOT satisfied.
  bool positive_test_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestConditionAttributeResponseHeaders);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_ATTRIBUTE_H_
