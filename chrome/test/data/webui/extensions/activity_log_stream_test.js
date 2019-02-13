// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for activity-log-item. */
suite('ExtensionsActivityLogStreamTest', function() {
  /**
   * Extension activityLogStream created before each test.
   * @type {extensions.ActivityLogStream}
   */
  let activityLogStream;
  let testVisible;

  // Initialize an extension activity log item before each test.
  setup(function() {
    PolymerTest.clearBody();

    activityLogStream = new extensions.ActivityLogStream();
    testVisible = extension_test_util.testVisible.bind(null, activityLogStream);

    document.body.appendChild(activityLogStream);
  });

  teardown(function() {
    activityLogStream.remove();
  });

  test('button toggles stream on/off', function() {
    Polymer.dom.flush();

    // Stream should be on when element is first attached to the DOM.
    testVisible('#activity-log-stream-header', true);
    testVisible('#empty-stream-message', true);
    expectTrue(activityLogStream.isStreamOnForTest());

    activityLogStream.$$('#toggle-stream-button').click();
    expectFalse(activityLogStream.isStreamOnForTest());
  });
});
