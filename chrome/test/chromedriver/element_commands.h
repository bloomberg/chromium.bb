// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_ELEMENT_COMMANDS_H_
#define CHROME_TEST_CHROMEDRIVER_ELEMENT_COMMANDS_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
class Value;
}

struct Session;
class Status;
class WebView;

typedef base::Callback<Status(
    Session* session,
    WebView* web_view,
    const std::string&,
    const base::DictionaryValue&,
    scoped_ptr<base::Value>*)> ElementCommand;

// Execute a command on a specific element.
Status ExecuteElementCommand(
    ElementCommand command,
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Search for an element on the page, starting from the given element.
Status ExecuteFindChildElement(
    int interval_ms,
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Search for multiple elements on the page, starting from the given element.
Status ExecuteFindChildElements(
    int interval_ms,
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Move the mouse to the given element.
Status ExecuteHoverOverElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Click on the element.
Status ExecuteClickElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Clear a TEXTAREA or text INPUT element's value.
Status ExecuteClearElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Send a sequence of key strokes to an element.
Status ExecuteSendKeysToElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

#endif  // CHROME_TEST_CHROMEDRIVER_ELEMENT_COMMANDS_H_
