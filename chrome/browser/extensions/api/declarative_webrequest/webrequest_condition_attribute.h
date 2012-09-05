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

class HeaderMatcher;

// Base class for all condition attributes of the declarative Web Request API
// except for condition attribute to test URLPatterns.
class WebRequestConditionAttribute {
 public:
  enum Type {
    CONDITION_RESOURCE_TYPE,
    CONDITION_CONTENT_TYPE,
    CONDITION_RESPONSE_HEADERS
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

// Condition attribute for matching against response headers. Uses HeaderMatcher
// to handle the actual tests, in connection with a boolean positiveness
// flag. If that flag is set to true, then IsFulfilled() returns true iff
// |header_matcher_| matches at least one header. Otherwise IsFulfilled()
// returns true iff the |header_matcher_| matches no header.
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
  WebRequestConditionAttributeResponseHeaders(
      scoped_ptr<const HeaderMatcher>* header_matcher, bool positive);

  const scoped_ptr<const HeaderMatcher> header_matcher_;
  const bool positive_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestConditionAttributeResponseHeaders);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_ATTRIBUTE_H_
