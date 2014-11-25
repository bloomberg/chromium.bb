// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements experimental API for <webview>.
// See web_view.js for details.
//
// <webview> Experimental API is only available on canary and dev channels of
// Chrome.

var WebViewImpl = require('webView').WebViewImpl;
var WebViewInternal = require('webViewInternal').WebViewInternal;

// Loads a data URL with a specified base URL used for relative links.
// Optionally, a virtual URL can be provided to be shown to the user instead
// of the data URL.
WebViewImpl.prototype.loadDataWithBaseUrl = function(
    dataUrl, baseUrl, virtualUrl) {
  if (!this.guest.getId()) {
    return;
  }
  WebViewInternal.loadDataWithBaseUrl(
      this.guest.getId(), dataUrl, baseUrl, virtualUrl, function() {
        // Report any errors.
        if (chrome.runtime.lastError != undefined) {
          window.console.error(
              'Error while running webview.loadDataWithBaseUrl: ' +
                  chrome.runtime.lastError.message);
        }
      });
};

// An array of <webview>'s experimental API methods. See |WEB_VIEW_API_METHODS|
// in web_view_api_methods.js for more details.
var WEB_VIEW_EXPERIMENTAL_API_METHODS = [
  'loadDataWithBaseUrl'
];

// Registers the experimantal WebVIew API when available.
WebViewImpl.maybeGetExperimentalApiMethods = function() {
  return WEB_VIEW_EXPERIMENTAL_API_METHODS;
};
