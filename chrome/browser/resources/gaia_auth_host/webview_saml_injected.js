// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Global injection guard that starts as undefined and sets to true after the
 * first run. This is for webview.executeScript injection where the execution
 * environment remains the same for multiple injections.
 * @type {boolean}
 */
var samlInjectedInitialized;

(function() {
  if (samlInjectedInitialized)
    return;
  samlInjectedInitialized = true;

  <include src="post_message_channel.js">
  <include src="../gaia_auth/saml_injected.js">
})();
