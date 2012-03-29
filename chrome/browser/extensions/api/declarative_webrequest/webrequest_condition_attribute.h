// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_ATTRIBUTE_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_ATTRIBUTE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/api/experimental.declarative.h"

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
    CONDITION_HAS_SCHEME
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

  // Returns a bit vector representing extensions::RequestStages. The bit vector
  // contains a 1 for each request stage during which the condition attribute
  // can be tested.
  virtual int GetStages() const = 0;

  // Returns whether the condition is fulfilled for this request.
  virtual bool IsFulfilled(net::URLRequest* request) = 0;

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

// Condition that checks whether a URL has a specific scheme.
// TODO(battre): Generalize this to allow checking for multiple schemes.
// TODO(battre): Alternatively, move the scheme check into the URLMatcher.
class WebRequestConditionAttributeHasScheme
    : public WebRequestConditionAttribute {
 public:
  virtual ~WebRequestConditionAttributeHasScheme();

  static bool IsMatchingType(const std::string& instance_type);

  // Factory method, see WebRequestConditionAttribute::Create.
  static scoped_ptr<WebRequestConditionAttribute> Create(
      const std::string& name,
      const base::Value* value,
      std::string* error);

  // Implementation of WebRequestConditionAttribute:
  virtual int GetStages() const OVERRIDE;
  virtual bool IsFulfilled(net::URLRequest* request) OVERRIDE;
  virtual Type GetType() const OVERRIDE;

 private:
  explicit WebRequestConditionAttributeHasScheme(const std::string& pattern);

  std::string pattern_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestConditionAttributeHasScheme);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_ATTRIBUTE_H_
