// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The <webview> custom element.

var registerElement = require('guestViewContainerElement').registerElement;
var forwardApiMethods = require('guestViewContainerElement').forwardApiMethods;
var GuestViewContainerElement =
    require('guestViewContainerElement').GuestViewContainerElement;
var WebViewImpl = require('webView').WebViewImpl;
var WEB_VIEW_API_METHODS = require('webViewApiMethods').WEB_VIEW_API_METHODS;

class WebViewElement extends GuestViewContainerElement {}

// Forward WebViewElement.foo* method calls to WebViewImpl.foo*.
forwardApiMethods(WebViewElement, WEB_VIEW_API_METHODS);

registerElement('WebView', WebViewElement, WebViewImpl);
