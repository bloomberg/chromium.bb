// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_ELEMENT_UTIL_H_
#define CHROME_TEST_CHROMEDRIVER_ELEMENT_UTIL_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

struct Session;
class Status;

struct WebPoint {
  WebPoint();
  WebPoint(int x, int y);
  ~WebPoint();

  void offset(int x_, int y_);

  int x;
  int y;
};

struct WebSize {
  WebSize();
  WebSize(int width, int height);
  ~WebSize();

  int width;
  int height;
};

struct WebRect {
  WebRect();
  WebRect(int x, int y, int width, int height);
  WebRect(const WebPoint& origin, const WebSize& size);
  ~WebRect();

  int x();
  int y();
  int width();
  int height();

  WebPoint origin;
  WebSize size;
};

base::DictionaryValue* CreateElement(const std::string& id);

// |id| could be null when no root element is given.
Status FindElement(
    int interval_ms,
    bool only_one,
    const std::string* root_element_id,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// |is_clickable| could be null.
// If not null, it's set to indicate whether center of the element is clickable.
Status GetElementClickableLocation(
    Session* session,
    const std::string& element_id,
    WebPoint* location,
    bool* is_clickable);

Status GetElementRegion(
    Session* session,
    const std::string& id,
    WebRect* rect);

Status GetElementTagName(
    Session* session,
    const std::string& id,
    std::string* name);

Status GetElementSize(
    Session* session,
    const std::string& id,
    WebSize* size);

Status IsElementClickable(
    Session* session,
    const std::string& id,
    WebPoint* location,
    bool* is_clickable);

Status IsElementDisplayed(
    Session* session,
    const std::string& id,
    bool ignore_opacity,
    bool* is_displayed);

Status IsOptionElementSelected(
    Session* session,
    const std::string& id,
    bool* is_selected);

Status IsOptionElementTogglable(
    Session* session,
    const std::string& id,
    bool* is_togglable);

Status SetOptionElementSelected(
    Session* session,
    const std::string& id,
    bool selected);

Status ToggleOptionElement(
    Session* session,
    const std::string& id);

Status ScrollElementRegionIntoView(
    Session* session,
    const std::string& id,
    WebRect* region,
    bool center,
    WebPoint* location);

#endif  // CHROME_TEST_CHROMEDRIVER_ELEMENT_UTIL_H_
