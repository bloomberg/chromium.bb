// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements experimental API for <webview>.
// See web_view.js for details.
//
// <webview> Experimental API is only available on canary and dev channels of
// Chrome.

var WebViewInternal = require('webView').WebViewInternal;
var WebView = require('webViewInternal').WebView;

WebViewInternal.prototype.maybeGetExperimentalEvents = function() {
  return {};
};

/** @private */
WebViewInternal.prototype.maybeGetExperimentalPermissions = function() {
  return [];
};

/** @private */
WebViewInternal.prototype.captureVisibleRegion = function(spec, callback) {
  WebView.captureVisibleRegion(this.guestInstanceId, spec, callback);
};

/** @private */
WebViewInternal.prototype.loadDataWithBaseUrl = function(
    dataUrl, baseUrl, virtualUrl) {
  if (!this.guestInstanceId) {
    return;
  }
  WebView.loadDataWithBaseUrl(
      this.guestInstanceId, dataUrl, baseUrl, virtualUrl, function () {
        // Report any errors.
        if (chrome.runtime.lastError != undefined) {
          window.console.error(
              "Error while running webview.loadDataWithBaseUrl: " +
                  chrome.runtime.lastError.message);
        }
      });
};

WebViewInternal.maybeRegisterExperimentalAPIs = function(proto) {
  proto.captureVisibleRegion = function(spec, callback) {
    privates(this).internal.captureVisibleRegion(spec, callback);
  };

  proto.loadDataWithBaseUrl = function(dataUrl, baseUrl, virtualUrl) {
    privates(this).internal.loadDataWithBaseUrl(dataUrl, baseUrl, virtualUrl);
  }
};
