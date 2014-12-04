// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @extends {testing.Test}
 */
function OptionsBrowsertestBase() {}

OptionsBrowsertestBase.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  runAccessibilityChecks: true,

  /** @override */
  accessibilityIssuesAreErrors: true,

  /** @override */
  setUp: function() {
    // user-image-stream is a streaming video element used for capturing a
    // user image during OOBE.
    this.accessibilityAuditConfig.ignoreSelectors('videoWithoutCaptions',
                                                  '.user-image-stream');
  },
};
