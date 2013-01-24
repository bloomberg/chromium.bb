// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/element_util.h"

#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome.h"
#include "chrome/test/chromedriver/js.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/status.h"
#include "third_party/webdriver/atoms.h"

namespace {

const char kElementKey[] = "ELEMENT";

bool ParseFromValue(base::Value* value, WebPoint* point) {
  base::DictionaryValue* dict_value;
  if (!value->GetAsDictionary(&dict_value))
    return false;
  double x, y;
  if (!dict_value->GetDouble("x", &x) ||
      !dict_value->GetDouble("y", &y))
    return false;
  point->x = static_cast<int>(x);
  point->y = static_cast<int>(y);
  return true;
}

base::Value* CreateValueFrom(WebPoint* point) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("x", point->x);
  dict->SetInteger("y", point->y);
  return dict;
}

bool ParseFromValue(base::Value* value, WebSize* size) {
  base::DictionaryValue* dict_value;
  if (!value->GetAsDictionary(&dict_value))
    return false;
  double width, height;
  if (!dict_value->GetDouble("width", &width) ||
      !dict_value->GetDouble("height", &height))
    return false;
  size->width = static_cast<int>(width);
  size->height = static_cast<int>(height);
  return true;
}

base::Value* CreateValueFrom(WebSize* size) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("width", size->width);
  dict->SetInteger("height", size->height);
  return dict;
}

bool ParseFromValue(base::Value* value, WebRect* rect) {
  base::DictionaryValue* dict_value;
  if (!value->GetAsDictionary(&dict_value))
    return false;
  double x, y, width, height;
  if (!dict_value->GetDouble("left", &x) ||
      !dict_value->GetDouble("top", &y) ||
      !dict_value->GetDouble("width", &width) ||
      !dict_value->GetDouble("height", &height))
    return false;
  rect->origin.x = static_cast<int>(x);
  rect->origin.y = static_cast<int>(y);
  rect->size.width = static_cast<int>(width);
  rect->size.height = static_cast<int>(height);
  return true;
}

base::Value* CreateValueFrom(WebRect* rect) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("left", rect->origin.x);
  dict->SetInteger("top", rect->origin.y);
  dict->SetInteger("width", rect->size.width);
  dict->SetInteger("height", rect->size.height);
  return dict;
}

Status CallAtomsJs(
    Session* session,
    const char* const* atom_function,
    const base::ListValue& args,
    scoped_ptr<base::Value>* result) {
  return session->chrome->CallFunction(
      session->frame, webdriver::atoms::asString(atom_function),
      args, result);
}

}  // namespace


WebPoint::WebPoint() : x(0), y(0) {}

WebPoint::WebPoint(int x, int y) : x(x), y(y) {}

WebPoint::~WebPoint() {}

void WebPoint::offset(int x_, int y_) {
  x += x_;
  y += y_;
}

WebSize::WebSize() : width(0), height(0) {}

WebSize::WebSize(int width, int height) : width(width), height(height) {}

WebSize::~WebSize() {}

WebRect::WebRect() : origin(0, 0), size(0, 0) {}

WebRect::WebRect(int x, int y, int width, int height)
    : origin(x, y), size(width, height) {}

WebRect::WebRect(const WebPoint& origin, const WebSize& size)
    : origin(origin), size(size) {}

WebRect::~WebRect() {}

int WebRect::x() { return origin.x; }

int WebRect::y() { return origin.y; }

int WebRect::width() { return size.width; }

int WebRect::height() { return size.height; }

base::DictionaryValue* CreateElement(const std::string& id) {
  base::DictionaryValue* element = new base::DictionaryValue();
  element->SetString(kElementKey, id);
  return element;
}

Status FindElement(
    int interval_ms,
    bool only_one,
    const std::string* root_element_id,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string strategy;
  if (!params.GetString("using", &strategy))
    return Status(kUnknownError, "'using' must be a string");
  std::string target;
  if (!params.GetString("value", &target))
    return Status(kUnknownError, "'value' must be a string");

  std::string script;
  if (only_one)
    script = webdriver::atoms::asString(webdriver::atoms::FIND_ELEMENT);
  else
    script = webdriver::atoms::asString(webdriver::atoms::FIND_ELEMENTS);
  scoped_ptr<base::DictionaryValue> locator(new base::DictionaryValue());
  locator->SetString(strategy, target);
  base::ListValue arguments;
  arguments.Append(locator.release());
  if (root_element_id)
    arguments.Append(CreateElement(*root_element_id));

  base::Time start_time = base::Time::Now();
  while (true) {
    scoped_ptr<base::Value> temp;
    Status status = session->chrome->CallFunction(
        session->frame, script, arguments, &temp);
    if (status.IsError())
      return status;

    if (!temp->IsType(base::Value::TYPE_NULL)) {
      if (only_one) {
        value->reset(temp.release());
        return Status(kOk);
      } else {
        base::ListValue* result;
        if (!temp->GetAsList(&result))
          return Status(kUnknownError, "script returns unexpected result");

        if (result->GetSize() > 0U) {
          value->reset(temp.release());
          return Status(kOk);
        }
      }
    }

    if ((base::Time::Now() - start_time).InMilliseconds() >=
        session->implicit_wait) {
      if (only_one) {
        return Status(kNoSuchElement);
      } else {
        value->reset(new base::ListValue());
        return Status(kOk);
      }
    }
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(interval_ms));
  }

  return Status(kUnknownError);
}

Status GetElementClickableLocation(
    Session* session,
    const std::string& element_id,
    WebPoint* location,
    bool* is_clickable) {
  bool is_displayed = false;
  Status status = IsElementDisplayed(session, element_id, true, &is_displayed);
  if (status.IsError())
    return status;
  if (!is_displayed)
    return Status(kUnknownError, "element must be displayed");

  WebRect rect;
  status = GetElementRegion(session, element_id, &rect);
  if (status.IsError())
    return status;

  if (rect.width() == 0 || rect.height() == 0)
    return Status(kElementNotVisible);

  status = ScrollElementRegionIntoView(
      session, element_id, &rect, true, location);
  if (status.IsError())
    return status;
  location->offset(rect.width() / 2, rect.height() / 2);
  if (is_clickable)
    return IsElementClickable(session, element_id, location, is_clickable);
  else
    return Status(kOk);
}

Status GetElementRegion(
    Session* session,
    const std::string& id,
    WebRect* rect) {
  base::ListValue args;
  args.Append(CreateElement(id));
  scoped_ptr<base::Value> result;
  Status status = session->chrome->CallFunction(
      session->frame, kGetElementRegionScript, args, &result);
  if (status.IsError())
    return status;
  if (!ParseFromValue(result.get(), rect)) {
    return Status(kUnknownError,
        "fail to parse value of getElementRegion");
  }
  return Status(kOk);
}

Status GetElementTagName(
    Session* session,
    const std::string& id,
    std::string* name) {
  base::ListValue args;
  args.Append(CreateElement(id));
  scoped_ptr<base::Value> result;
  Status status = session->chrome->CallFunction(
      session->frame, "function(elem) { return elem.tagName.toLowerCase(); }",
      args, &result);
  if (status.IsError())
    return status;
  if (!result->GetAsString(name))
    return Status(kUnknownError, "fail to get element tag name");
  return Status(kOk);
}

Status GetElementSize(
    Session* session,
    const std::string& id,
    WebSize* size) {
  base::ListValue args;
  args.Append(CreateElement(id));
  scoped_ptr<base::Value> result;
  Status status = CallAtomsJs(
      session, webdriver::atoms::GET_SIZE, args, &result);
  if (status.IsError())
    return status;
  if (!ParseFromValue(result.get(), size))
    return Status(kUnknownError, "fail to parse value of GET_SIZE");
  return Status(kOk);
}

Status IsElementClickable(
    Session* session,
    const std::string& id,
    WebPoint* location,
    bool* is_clickable) {
  base::ListValue args;
  args.Append(CreateElement(id));
  args.Append(CreateValueFrom(location));
  scoped_ptr<base::Value> result;
  Status status = CallAtomsJs(
      session, webdriver::atoms::IS_ELEMENT_CLICKABLE, args, &result);
  if (status.IsError())
    return status;
  base::DictionaryValue* dict;
  if (!result->GetAsDictionary(&dict) ||
      !dict->GetBoolean("clickable", is_clickable))
    return Status(kUnknownError, "fail to parse value of IS_ELEMENT_CLICKABLE");

  if (!*is_clickable) {
    std::string message;
    if (!dict->GetString("message", &message))
      message = "element is not clickable";
    return Status(kOk, message);
  }
  return Status(kOk);
}

Status IsElementDisplayed(
    Session* session,
    const std::string& id,
    bool ignore_opacity,
    bool* is_displayed) {
  base::ListValue args;
  args.Append(CreateElement(id));
  args.AppendBoolean(ignore_opacity);
  scoped_ptr<base::Value> result;
  Status status = CallAtomsJs(
      session, webdriver::atoms::IS_DISPLAYED, args, &result);
  if (status.IsError())
    return status;
  if (!result->GetAsBoolean(is_displayed))
    return Status(kUnknownError, "IS_DISPLAYED should return a boolean value");
  return Status(kOk);
}

Status IsOptionElementSelected(
    Session* session,
    const std::string& id,
    bool* is_selected) {
  base::ListValue args;
  args.Append(CreateElement(id));
  scoped_ptr<base::Value> result;
  Status status = CallAtomsJs(
      session, webdriver::atoms::IS_SELECTED, args, &result);
  if (status.IsError())
    return status;
  if (!result->GetAsBoolean(is_selected))
    return Status(kUnknownError, "IS_SELECTED should return a boolean value");
  return Status(kOk);
}

Status IsOptionElementTogglable(
    Session* session,
    const std::string& id,
    bool* is_togglable) {
  base::ListValue args;
  args.Append(CreateElement(id));
  scoped_ptr<base::Value> result;
  Status status = session->chrome->CallFunction(
      session->frame, kIsOptionElementToggleableScript, args, &result);
  if (status.IsError())
    return status;
  if (!result->GetAsBoolean(is_togglable))
    return Status(kUnknownError, "fail check if option togglable or not");
  return Status(kOk);
}

Status SetOptionElementSelected(
    Session* session,
    const std::string& id,
    bool selected) {
  // TODO(171034): need to fix throwing error if an alert is triggered.
  base::ListValue args;
  args.Append(CreateElement(id));
  args.AppendBoolean(selected);
  scoped_ptr<base::Value> result;
  return CallAtomsJs(
      session, webdriver::atoms::CLICK, args, &result);
}

Status ToggleOptionElement(
    Session* session,
    const std::string& id) {
  bool is_selected;
  Status status = IsOptionElementSelected(session, id, &is_selected);
  if (status.IsError())
    return status;
  return SetOptionElementSelected(session, id, !is_selected);
}

Status ScrollElementRegionIntoView(
    Session* session,
    const std::string& id,
    WebRect* region,
    bool center,
    WebPoint* location) {
  WebPoint region_offset = region->origin;
  base::ListValue args;
  args.Append(CreateElement(id));
  args.AppendBoolean(center);
  args.Append(CreateValueFrom(region));
  scoped_ptr<base::Value> result;

  // TODO(chrisgao): Nested frame. See http://crbug.com/170998.
  Status status = CallAtomsJs(
      session, webdriver::atoms::GET_LOCATION_IN_VIEW, args, &result);
  if (status.IsError())
    return status;
  if (!ParseFromValue(result.get(), &region_offset))
    return Status(kUnknownError, "fail to parse value of GET_LOCATION_IN_VIEW");
  *location = region_offset;
  return Status(kOk);
}
