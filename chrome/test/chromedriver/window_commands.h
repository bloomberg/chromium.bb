// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_WINDOW_COMMANDS_H_
#define CHROME_TEST_CHROMEDRIVER_WINDOW_COMMANDS_H_

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
    const base::DictionaryValue&,
    scoped_ptr<base::Value>*)> WindowCommand;

// Execute a Window Command on the target window.
Status ExecuteWindowCommand(
    WindowCommand command,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Loads a URL.
Status ExecuteGet(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Evaluates a given script with arguments.
Status ExecuteExecuteScript(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Changes the targeted frame for the given session.
Status ExecuteSwitchToFrame(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Get the current page title.
Status ExecuteGetTitle(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Get the current page source.
Status ExecuteGetPageSource(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Search for an element on the page, starting from the document root.
Status ExecuteFindElement(
    int interval_ms,
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Search for multiple elements on the page, starting from the document root.
Status ExecuteFindElements(
    int interval_ms,
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Get the current page url.
Status ExecuteGetCurrentUrl(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Navigate backward in the browser history.
Status ExecuteGoBack(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Navigate forward in the browser history.
Status ExecuteGoForward(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Refresh the current page.
Status ExecuteRefresh(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Move the mouse by an offset of the element if specified .
Status ExecuteMouseMoveTo(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Click a mouse button at the coordinates set by the last moveto.
Status ExecuteMouseClick(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Click and hold a mouse button at the coordinates set by the last moveto.
Status ExecuteMouseButtonDown(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Releases the mouse button previously held (where the mouse is currently at).
Status ExecuteMouseButtonUp(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Double-clicks at the current mouse coordinates (set by last moveto).
Status ExecuteMouseDoubleClick(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

Status ExecuteGetActiveElement(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Gets the status of the application cache (window.applicationCache.status).
Status ExecuteGetAppCacheStatus(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

Status ExecuteIsBrowserOnline(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

Status ExecuteGetStorageItem(
    const char* storage,
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

Status ExecuteGetStorageKeys(
    const char* storage,
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

Status ExecuteSetStorageItem(
    const char* storage,
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

Status ExecuteRemoveStorageItem(
    const char* storage,
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

Status ExecuteClearStorage(
    const char* storage,
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

Status ExecuteGetStorageSize(
    const char* storage,
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

Status ExecuteScreenshot(
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

#endif  // CHROME_TEST_CHROMEDRIVER_WINDOW_COMMANDS_H_
