// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module provides --site-per-process overrides for WebView (<webview>).
// See web_view.js for details.

var IdGenerator = requireNative('id_generator');
var WebViewImpl = require('webView').WebViewImpl;
// NOTE: Do not remove these, we implicitly depend on these in
// --site-per-process.
var GuestViewIframe = require('guestViewIframe');
var GuestViewIframeContainer = require('guestViewIframeContainer');

WebViewImpl.prototype.attachWindow$ = function(opt_guestInstanceId) {
  // If |opt_guestInstanceId| was provided, then a different existing guest is
  // being attached to this webview, and the current one will get destroyed.
  if (opt_guestInstanceId) {
    if (this.guest.getId() == opt_guestInstanceId) {
      return true;
    }
    this.guest.destroy();
    this.guest = new GuestView('webview', opt_guestInstanceId);
  }

  var generatedId = IdGenerator.GetNextId();
  // Generate an instance id for the container.
  this.onInternalInstanceId(generatedId);
  return true;
};
