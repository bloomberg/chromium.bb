// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements experimental API for <webview>.
// See web_view.js and web_view_api_methods.js for details.
//
// <webview> Experimental API is only available on canary and channels of
// Chrome.

var WebViewImpl = require('webView').WebViewImpl;
var WebViewInternal = require('webViewInternal').WebViewInternal;

// An array of <webview>'s experimental API methods.  See |WEB_VIEW_API_METHODS|
// in web_view_api_methods.js for more details.
var WEB_VIEW_EXPERIMENTAL_API_METHODS = [
  // Captures the visible region of the WebView contents into a bitmap.
  'captureVisibleRegion'
];

// Registers the experimantal WebVIew API when available.
WebViewImpl.maybeGetExperimentalApiMethods = function() {
  return WEB_VIEW_EXPERIMENTAL_API_METHODS;
};
