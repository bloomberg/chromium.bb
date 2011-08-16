// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_basic_types.h"

#include <cmath>

#include "base/values.h"

using base::Value;

namespace webdriver {

Point::Point() { }

Point::Point(double x, double y) : x_(x), y_(y) { }

Point::~Point() { }

void Point::Offset(double x, double y) {
  x_ += x;
  y_ += y;
}

double Point::x() const {
  return x_;
}

double Point::y() const {
  return y_;
}

int Point::rounded_x() const {
  int truncated = static_cast<int>(x_);
  if (std::abs(x_ - truncated) < 0.5)
    return truncated;
  return truncated + 1;
}

int Point::rounded_y() const {
  int truncated = static_cast<int>(y_);
  if (std::abs(y_ - truncated) < 0.5)
    return truncated;
  return truncated + 1;
}

Size::Size() { }

Size::Size(double width, double height) : width_(width), height_(height) { }

Size::~Size() { }

double Size::width() const {
  return width_;
}

double Size::height() const {
  return height_;
}

Rect::Rect() { }

Rect::Rect(double x, double y, double width, double height)
    : origin_(x, y), size_(width, height) { }

Rect::Rect(const Point& origin, const Size& size)
    : origin_(origin), size_(size) { }

Rect::~Rect() { }

const Point& Rect::origin() const {
  return origin_;
}

const Size& Rect::size() const {
  return size_;
}

double Rect::x() const {
  return origin_.x();
}

double Rect::y() const {
  return origin_.y();
}

double Rect::width() const {
  return size_.width();
}

double Rect::height() const {
  return size_.height();
}

}  // namespace webdriver

Value* ValueConversionTraits<webdriver::Point>::CreateValueFrom(
    const webdriver::Point& t) {
  DictionaryValue* value = new DictionaryValue();
  value->SetDouble("x", t.x());
  value->SetDouble("y", t.y());
  return value;
}

bool ValueConversionTraits<webdriver::Point>::SetFromValue(
    const Value* value, webdriver::Point* t) {
  if (!value->IsType(Value::TYPE_DICTIONARY))
    return false;

  const DictionaryValue* dict_value =
      static_cast<const DictionaryValue*>(value);
  double x, y;
  if (!dict_value->GetDouble("x", &x) ||
      !dict_value->GetDouble("y", &y))
    return false;
  *t = webdriver::Point(x, y);
  return true;
}

bool ValueConversionTraits<webdriver::Point>::CanConvert(const Value* value) {
  webdriver::Point t;
  return SetFromValue(value, &t);
}

Value* ValueConversionTraits<webdriver::Size>::CreateValueFrom(
    const webdriver::Size& t) {
  DictionaryValue* value = new DictionaryValue();
  value->SetDouble("width", t.width());
  value->SetDouble("height", t.height());
  return value;
}

bool ValueConversionTraits<webdriver::Size>::SetFromValue(
    const Value* value, webdriver::Size* t) {
  if (!value->IsType(Value::TYPE_DICTIONARY))
    return false;

  const DictionaryValue* dict_value =
      static_cast<const DictionaryValue*>(value);
  double width, height;
  if (!dict_value->GetDouble("width", &width) ||
      !dict_value->GetDouble("height", &height))
    return false;
  *t = webdriver::Size(width, height);
  return true;
}

bool ValueConversionTraits<webdriver::Size>::CanConvert(const Value* value) {
  webdriver::Size t;
  return SetFromValue(value, &t);
}

Value* ValueConversionTraits<webdriver::Rect>::CreateValueFrom(
    const webdriver::Rect& t) {
  DictionaryValue* value = new DictionaryValue();
  value->SetDouble("left", t.x());
  value->SetDouble("top", t.y());
  value->SetDouble("width", t.width());
  value->SetDouble("height", t.height());
  return value;
}

bool ValueConversionTraits<webdriver::Rect>::SetFromValue(
    const Value* value, webdriver::Rect* t) {
  if (!value->IsType(Value::TYPE_DICTIONARY))
    return false;

  const DictionaryValue* dict_value =
      static_cast<const DictionaryValue*>(value);
  double x, y, width, height;
  if (!dict_value->GetDouble("left", &x) ||
      !dict_value->GetDouble("top", &y) ||
      !dict_value->GetDouble("width", &width) ||
      !dict_value->GetDouble("height", &height))
    return false;
  *t = webdriver::Rect(x, y, width, height);
  return true;
}

bool ValueConversionTraits<webdriver::Rect>::CanConvert(const Value* value) {
  webdriver::Rect t;
  return SetFromValue(value, &t);
}
