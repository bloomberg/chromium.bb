// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('gaia-whitelist-error', (function() {
  // The help topic regarding user not being in the whitelist.
  /** @const */ var HELP_CANT_ACCESS_ACCOUNT = 188036;

  return {
    enterpriseManaged: false,

    onLearnMoreClicked: function() {
      chrome.send('launchHelpApp', [HELP_CANT_ACCESS_ACCOUNT]);
    },

    tryAgainButtonClicked: function() {
      $('gaia-signin').showWhitelistCheckFailedError(false);
    }
  };
})());
