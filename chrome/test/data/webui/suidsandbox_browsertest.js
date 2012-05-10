// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for SUID Sandbox testing.
 * @extends {testing.Test}
 * @constructor
 */
function SUIDSandboxUITest() {}

SUIDSandboxUITest.prototype = {
  __proto__: testing.Test.prototype,
  /**
   * Browse to the options page & call our preLoad().
   */
  browsePreload: 'chrome://sandbox',

};

// This test is for Linux only.
GEN('#if defined(OS_LINUX)');
GEN('#define MAYBE_testSUIDSandboxEnabled \\');
GEN('    FLAKY_testSUIDSandboxEnabled');
GEN('#else');
GEN('#define MAYBE_testSUIDSandboxEnabled \\');
GEN('    DISABLED_testSUIDSandboxEnabled');
GEN('#endif');

/**
 * Test if the SUID sandbox is enabled.
 *
 * TODO(bradchen): Remove FLAKY_ and flip polarity of this test once
 * the SUID sandbox is enabled on the Chromium bots. In the mean time
 * this test will make it clear that the enabling steps are effective.
 */
TEST_F('SUIDSandboxUITest', 'MAYBE_testSUIDSandboxEnabled', function() {
    var suidyesstring = 'SUID Sandbox\tYes';
    var suidnostring = 'SUID Sandbox\tNo';
    var suidyes = document.body.innerText.match(suidyesstring);
    var suidno = document.body.innerText.match(suidnostring);

    expectEquals(null, suidyes);
    expectFalse(suidno == null);
    expectEquals(suidnostring, suidno[0]);
});
