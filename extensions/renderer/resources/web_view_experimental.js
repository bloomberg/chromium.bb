// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements experimental API for <webview>.
// See web_view.js for details.
//
// <webview> Experimental API is only available on canary and dev channels of
// Chrome.

var WebViewInternal = require('webView').WebViewInternal;

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

WebViewInternal.maybeRegisterExperimentalAPIs = function(proto) {
  proto.captureVisibleRegion = function(spec, callback) {
    privates(this).internal.captureVisibleRegion(spec, callback);
  };
};
