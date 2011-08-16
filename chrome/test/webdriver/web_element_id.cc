// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/web_element_id.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/test/automation/javascript_message_utils.h"

namespace {

// Special dictionary key to identify an element ID to WebDriver atoms and
// remote clients.
const char kWebElementKey[] = "ELEMENT";

}  // namespace

namespace webdriver {

const char LocatorType::kClassName[] = "className";
const char LocatorType::kCss[] = "css";
const char LocatorType::kId[] = "id";
const char LocatorType::kLinkText[] = "linkText";
const char LocatorType::kName[] = "name";
const char LocatorType::kPartialLinkText[] = "partialLinkText";
const char LocatorType::kTagName[] = "tagName";
const char LocatorType::kXpath[] = "xpath";

WebElementId::WebElementId() : is_valid_(false) {}

WebElementId::WebElementId(const std::string& id) : id_(id), is_valid_(true) {}

WebElementId::WebElementId(const Value* value) {
  is_valid_ = false;
  if (value->IsType(Value::TYPE_DICTIONARY)) {
    is_valid_ = static_cast<const DictionaryValue*>(value)->
        GetString(kWebElementKey, &id_);
  }
}

WebElementId::~WebElementId() {}

Value* WebElementId::ToValue() const {
  CHECK(is_valid_);
  if (id_.empty())
    return Value::CreateNullValue();
  DictionaryValue* element = new DictionaryValue();
  element->SetString(kWebElementKey, id_);
  return element;
}

bool WebElementId::is_valid() const {
  return is_valid_;
}

}  // namespace webdriver

base::Value* ValueConversionTraits<webdriver::WebElementId>::CreateValueFrom(
    const webdriver::WebElementId& t) {
  return t.ToValue();
}

bool ValueConversionTraits<webdriver::WebElementId>::SetFromValue(
    const base::Value* value, webdriver::WebElementId* t) {
  webdriver::WebElementId id(value);
  if (id.is_valid())
    *t = id;
  return id.is_valid();
}

bool ValueConversionTraits<webdriver::WebElementId>::CanConvert(
    const base::Value* value) {
  webdriver::WebElementId t;
  return SetFromValue(value, &t);
}
