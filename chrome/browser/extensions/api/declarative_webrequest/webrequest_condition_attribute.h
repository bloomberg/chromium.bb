// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_ATTRIBUTE_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_ATTRIBUTE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stage.h"
#include "chrome/common/extensions/api/events.h"
#include "content/public/common/resource_type.h"

namespace base {
class Value;
}

namespace net {
class URLRequest;
}

namespace extensions {

class HeaderMatcher;
struct WebRequestData;

// Base class for all condition attributes of the declarative Web Request API
// except for condition attribute to test URLPatterns.
class WebRequestConditionAttribute
    : public base::RefCounted<WebRequestConditionAttribute> {
 public:
  enum Type {
    CONDITION_RESOURCE_TYPE,
    CONDITION_CONTENT_TYPE,
    CONDITION_RESPONSE_HEADERS,
    CONDITION_THIRD_PARTY,
    CONDITION_REQUEST_HEADERS,
    CONDITION_STAGES
  };

  WebRequestConditionAttribute();

  // Factory method that creates a WebRequestConditionAttribute for the JSON
  // dictionary {|name|: |value|} passed by the extension API. Sets |error| and
  // returns NULL if something fails.
  // The ownership of |value| remains at the caller.
  static scoped_refptr<const WebRequestConditionAttribute> Create(
      const std::string& name,
      const base::Value* value,
      std::string* error);

  // Returns a bit vector representing extensions::RequestStage. The bit vector
  // contains a 1 for each request stage during which the condition attribute
  // can be tested.
  virtual int GetStages() const = 0;

  // Returns whether the condition is fulfilled for this request.
  virtual bool IsFulfilled(
      const WebRequestData& request_data) const = 0;

  virtual Type GetType() const = 0;
  virtual std::string GetName() const = 0;

  // Compares the Type of two WebRequestConditionAttributes, needs to be
  // overridden for parameterized types.
  virtual bool Equals(const WebRequestConditionAttribute* other) const;

 protected:
  friend class base::RefCounted<WebRequestConditionAttribute>;
  virtual ~WebRequestConditionAttribute();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestConditionAttribute);
};

typedef std::vector<scoped_refptr<const WebRequestConditionAttribute> >
    WebRequestConditionAttributes;

//
// The following are concrete condition attributes.
//

// Condition that checks whether a request is for a specific resource type.
class WebRequestConditionAttributeResourceType
    : public WebRequestConditionAttribute {
 public:
  // Factory method, see WebRequestConditionAttribute::Create.
  static scoped_refptr<const WebRequestConditionAttribute> Create(
      const std::string& instance_type,
      const base::Value* value,
      std::string* error,
      bool* bad_message);

  // Implementation of WebRequestConditionAttribute:
  virtual int GetStages() const OVERRIDE;
  virtual bool IsFulfilled(
      const WebRequestData& request_data) const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual std::string GetName() const OVERRIDE;
  virtual bool Equals(const WebRequestConditionAttribute* other) const OVERRIDE;

 private:
  explicit WebRequestConditionAttributeResourceType(
      const std::vector<content::ResourceType>& types);
  virtual ~WebRequestConditionAttributeResourceType();

  const std::vector<content::ResourceType> types_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestConditionAttributeResourceType);
};

// Condition that checks whether a response's Content-Type header has a
// certain MIME media type.
class WebRequestConditionAttributeContentType
    : public WebRequestConditionAttribute {
 public:
  // Factory method, see WebRequestConditionAttribute::Create.
  static scoped_refptr<const WebRequestConditionAttribute> Create(
      const std::string& name,
      const base::Value* value,
      std::string* error,
      bool* bad_message);

  // Implementation of WebRequestConditionAttribute:
  virtual int GetStages() const OVERRIDE;
  virtual bool IsFulfilled(
      const WebRequestData& request_data) const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual std::string GetName() const OVERRIDE;
  virtual bool Equals(const WebRequestConditionAttribute* other) const OVERRIDE;

 private:
  explicit WebRequestConditionAttributeContentType(
      const std::vector<std::string>& include_content_types,
      bool inclusive);
  virtual ~WebRequestConditionAttributeContentType();

  const std::vector<std::string> content_types_;
  const bool inclusive_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestConditionAttributeContentType);
};

// Condition attribute for matching against request headers. Uses HeaderMatcher
// to handle the actual tests, in connection with a boolean positiveness
// flag. If that flag is set to true, then IsFulfilled() returns true iff
// |header_matcher_| matches at least one header. Otherwise IsFulfilled()
// returns true iff the |header_matcher_| matches no header.
class WebRequestConditionAttributeRequestHeaders
    : public WebRequestConditionAttribute {
 public:
  // Factory method, see WebRequestConditionAttribute::Create.
  static scoped_refptr<const WebRequestConditionAttribute> Create(
      const std::string& name,
      const base::Value* value,
      std::string* error,
      bool* bad_message);

  // Implementation of WebRequestConditionAttribute:
  virtual int GetStages() const OVERRIDE;
  virtual bool IsFulfilled(
      const WebRequestData& request_data) const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual std::string GetName() const OVERRIDE;
  virtual bool Equals(const WebRequestConditionAttribute* other) const OVERRIDE;

 private:
  WebRequestConditionAttributeRequestHeaders(
      scoped_ptr<const HeaderMatcher> header_matcher, bool positive);
  virtual ~WebRequestConditionAttributeRequestHeaders();

  const scoped_ptr<const HeaderMatcher> header_matcher_;
  const bool positive_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestConditionAttributeRequestHeaders);
};

// Condition attribute for matching against response headers. Uses HeaderMatcher
// to handle the actual tests, in connection with a boolean positiveness
// flag. If that flag is set to true, then IsFulfilled() returns true iff
// |header_matcher_| matches at least one header. Otherwise IsFulfilled()
// returns true iff the |header_matcher_| matches no header.
class WebRequestConditionAttributeResponseHeaders
    : public WebRequestConditionAttribute {
 public:
  // Factory method, see WebRequestConditionAttribute::Create.
  static scoped_refptr<const WebRequestConditionAttribute> Create(
      const std::string& name,
      const base::Value* value,
      std::string* error,
      bool* bad_message);

  // Implementation of WebRequestConditionAttribute:
  virtual int GetStages() const OVERRIDE;
  virtual bool IsFulfilled(
      const WebRequestData& request_data) const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual std::string GetName() const OVERRIDE;
  virtual bool Equals(const WebRequestConditionAttribute* other) const OVERRIDE;

 private:
  WebRequestConditionAttributeResponseHeaders(
      scoped_ptr<const HeaderMatcher> header_matcher, bool positive);
  virtual ~WebRequestConditionAttributeResponseHeaders();

  const scoped_ptr<const HeaderMatcher> header_matcher_;
  const bool positive_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestConditionAttributeResponseHeaders);
};

// This condition tests whether the request origin is third-party.
class WebRequestConditionAttributeThirdParty
    : public WebRequestConditionAttribute {
 public:
  // Factory method, see WebRequestConditionAttribute::Create.
  static scoped_refptr<const WebRequestConditionAttribute> Create(
      const std::string& name,
      const base::Value* value,
      std::string* error,
      bool* bad_message);

  // Implementation of WebRequestConditionAttribute:
  virtual int GetStages() const OVERRIDE;
  virtual bool IsFulfilled(
      const WebRequestData& request_data) const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual std::string GetName() const OVERRIDE;
  virtual bool Equals(const WebRequestConditionAttribute* other) const OVERRIDE;

 private:
  explicit WebRequestConditionAttributeThirdParty(bool match_third_party);
  virtual ~WebRequestConditionAttributeThirdParty();

  const bool match_third_party_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestConditionAttributeThirdParty);
};

// This condition is used as a filter for request stages. It is true exactly in
// stages specified on construction.
class WebRequestConditionAttributeStages
    : public WebRequestConditionAttribute {
 public:
  // Factory method, see WebRequestConditionAttribute::Create.
  static scoped_refptr<const WebRequestConditionAttribute> Create(
      const std::string& name,
      const base::Value* value,
      std::string* error,
      bool* bad_message);

  // Implementation of WebRequestConditionAttribute:
  virtual int GetStages() const OVERRIDE;
  virtual bool IsFulfilled(
      const WebRequestData& request_data) const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual std::string GetName() const OVERRIDE;
  virtual bool Equals(const WebRequestConditionAttribute* other) const OVERRIDE;

 private:
  explicit WebRequestConditionAttributeStages(int allowed_stages);
  virtual ~WebRequestConditionAttributeStages();

  const int allowed_stages_;  // Composition of RequestStage values.

  DISALLOW_COPY_AND_ASSIGN(WebRequestConditionAttributeStages);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_ATTRIBUTE_H_
