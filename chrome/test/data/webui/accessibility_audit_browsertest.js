// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests to ensure that the accessibility audit and mechanisms
 * to enable/disable it work as expected.
 * @author aboxhall@google.com (Alice Boxhall)
 * @see test_api.js
 */

/**
 * Test fixture for accessibility audit test.
 * @constructor
 * @extends testing.Test
 */
function WebUIAccessibilityAuditBrowserTest() {}

WebUIAccessibilityAuditBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload: 'chrome://terms',

  runAccessibilityChecks: true,
  accessibilityIssuesAreErrors: true,

  /**
   * Number of expected accessibility errors, if it should be checked, otherwise
   * null. Note: 'a11y' is short for 'accessibility'
   * @type {?number}
   */
  expectedWarnings: null,

  /**
   * Number of expected accessibility warnings, if it should be checked,
   * otherwise null.
   * @type {?number}
   */
  expectedErrors: null,

  tearDown: function() {
    if (this.expectedErrors != null)
      expectEquals(this.expectedErrors, this.getAccessibilityErrors().length);
    if (this.expectedWarnings != null) {
      expectEquals(this.expectedWarnings,
                   this.getAccessibilityWarnings().length);
    }
    testing.Test.prototype.tearDown.call(this);
  }
};

/**
 * Test fixture for tests that are expected to fail.
 */
function WebUIAccessibilityAuditBrowserTest_ShouldFail() {}

WebUIAccessibilityAuditBrowserTest_ShouldFail.prototype = {
  __proto__: WebUIAccessibilityAuditBrowserTest.prototype,

  testShouldFail: true
};

/**
 * Adds some canned audit failures to the page being tested:
 * - A low-contrast (white on white) element
 * - An element with a non-existent ARIA role
 * - A control without a label
 */
function addAuditFailures() {
  // Contrast ratio
  var style = document.createElement('style');
  style.innerText = 'p { color: #ffffff }';
  document.head.appendChild(style);

  // Bad ARIA role
  var div = document.createElement('div');
  div.setAttribute('role', 'not-a-role');
  document.body.appendChild(div);

  // Unlabelled control
  var input = document.createElement('input');
  input.type = 'text';
  document.body.appendChild(input);
}

/**
 * Adds an expectation that console.warn() will be called at least once with
 * a string matching '.*accessibility.*'.
 */
function expectReportConsoleWarning() {
  function StubConsole() {};
  StubConsole.prototype.warn = function() {};
  StubConsole.prototype.log = function() {};
  StubConsole.prototype.info = function() {};
  StubConsole.prototype.error = function() {};
  var mockConsole = mock(StubConsole);
  mockConsole.expects(atLeastOnce()).warn(stringContains('accessibility'));
  mockConsole.stubs().log(ANYTHING);
  mockConsole.stubs().info(ANYTHING);
  mockConsole.stubs().error(ANYTHING);
  console = mockConsole.proxy();
}

/**
 * Creates a mock axs.Audit object.
 * @return {object} a mock object with a run() method.
 */
function createMockAudit() {
  function StubAudit() {};
  StubAudit.prototype.run = function() {};
  return mock(StubAudit);
}

/**
 * Creates an expectation that the global axs.Audit object will never have its
 * run() method called.
 */
function expectAuditWillNotRun() {
  var audit = createMockAudit();
  audit.expects(never()).run();
  axs.Audit = audit.proxy();
}

/**
 * Creates an expectation that the global axs.Audit object will have its run()
 * method called |times| times.
 * This creates an interstitial mock axs.Audit object with the expectation, and
 * delegates to the real axs.Audit object to run the actual audit.
 * @param {number} times The number of times the audit is expected to run.
 */
function expectAuditWillRun(times) {
  var audit = createMockAudit();
  var realAudit = axs.Audit;
  var expectedInvocation = audit.expects(exactly(times)).run();
  var willArgs = [];
  for (var i = 0; i < times; i++)
    willArgs.push(callFunction(realAudit.run));
  expectedInvocation.will.apply(expectedInvocation, willArgs);
  axs.Audit = audit.proxy();
}

// Tests that an audit failure causes a test failure, if both
// |runAccessibilityChecks| and |accessibilityIssuesAreErrors| are true.
TEST_F('WebUIAccessibilityAuditBrowserTest_ShouldFail', 'testWithAuditFailures',
       function() {
  expectAuditWillRun(1);
  addAuditFailures();
});

// Tests that the accessibility audit does not run if |runAccessibilityChecks|
// is false.
TEST_F('WebUIAccessibilityAuditBrowserTest',
       'testWithAuditFailures_a11yChecksDisabled',
       function() {
  expectAuditWillNotRun();
  this.disableAccessibilityChecks();
  addAuditFailures();
});

// Tests that the accessibility audit will run but not cause a test failure when
// accessibilityIssuesAreErrors(false) is called in the test function
TEST_F('WebUIAccessibilityAuditBrowserTest',
       'testWithAuditFailures_a11yIssuesAreWarnings',
        function() {
  this.accessibilityIssuesAreErrors = false;
  expectAuditWillRun(1);
  expectReportConsoleWarning();

  this.expectedWarnings = 1;
  this.expectedErrors = 2;
  this.enableAccessibilityChecks();
  addAuditFailures();
});

/**
 * Test fixture with |runAccessibilityChecks| set to false.
 * @constructor
 * @extends {WebUIAccessibilityAuditBrowserTest}
 */
function WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture() {}

WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture.prototype = {
  __proto__: WebUIAccessibilityAuditBrowserTest.prototype,

  runAccessibilityChecks: false,
  accessibilityIssuesAreErrors: true,
};

/**
 * Test fixture with |runAccessibilityChecks| set to false for tests that should
 * fail.
 * @constructor
 * @extends {WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture}
 */
function WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture_ShouldFail()
{}

WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture_ShouldFail.prototype =
{
  __proto__:
      WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture.prototype,

  testShouldFail: true
};



// Tests that the accessibility audit does not run when |runAccessibilityChecks|
// is set to false in the test fixture.
TEST_F('WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture',
       'testWithAuditFailures_a11yChecksNotEnabled',
       function() {
  expectAuditWillNotRun();
  addAuditFailures();
});

// Tests that the accessibility audit does run if the
// enableAccessibilityChecks() method is called in the test function.
TEST_F('WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture_ShouldFail',
       'testWithAuditFailures',
       function() {
         console.log(axs.Audit);
  expectAuditWillRun(1);
  this.enableAccessibilityChecks();
  addAuditFailures();
});

// Tests that the accessibility audit runs when the expectAccessibilityOk()
// method is called.
TEST_F('WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture',
       'testRunningAuditManually_noErrors',
       function() {
  expectAuditWillRun(1);
  expectAccessibilityOk();
});

// Tests that calling expectAccessibilityOk() when there are accessibility
// issues on the page causes the test to fail.
TEST_F('WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture_ShouldFail',
       'testRunningAuditManually_withErrors',
       function() {
  expectAuditWillRun(1);
  addAuditFailures();
  expectAccessibilityOk();
});

// Tests that calling expectAccessibilityOk() multiple times will cause the
// accessibility audit to run multiple times.
TEST_F('WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture',
       'testRunningAuditManuallySeveralTimes', function() {
  expectAuditWillRun(2);
  expectAccessibilityOk();
  expectAccessibilityOk();
});

/**
 * Test fixture with |accessibilityIssuesAreErrors| set to false.
 * @constructor
 * @extends {WebUIAccessibilityAuditBrowserTest}
 */
function WebUIAccessibilityAuditBrowserTest_IssuesAreWarnings() {}

WebUIAccessibilityAuditBrowserTest_IssuesAreWarnings.prototype = {
  __proto__: WebUIAccessibilityAuditBrowserTest.prototype,

  accessibilityIssuesAreErrors: false,
};

/**
 * Test fixture with |accessibilityIssuesAreErrors| set to false for tests that
 * should fail
 * @constructor
 * @extends {WebUIAccessibilityAuditBrowserTest_IssuesAreWarnings}
 */
function WebUIAccessibilityAuditBrowserTest_IssuesAreWarnings_ShouldFail() {}

WebUIAccessibilityAuditBrowserTest_IssuesAreWarnings_ShouldFail.prototype = {
  __proto__: WebUIAccessibilityAuditBrowserTest_IssuesAreWarnings.prototype,

  testShouldFail: true
};


// Tests that the accessibility audit will run but not cause a test failure when
// |accessibilityIssuesAreErrors| is false in the test fixture.
TEST_F('WebUIAccessibilityAuditBrowserTest_IssuesAreWarnings',
       'testWithAuditFailures',
        function() {
  expectAuditWillRun(1);
  expectReportConsoleWarning();
  this.expectedWarnings = 1;
  this.expectedErrors = 2;

  this.enableAccessibilityChecks();
  addAuditFailures();
});

// Tests that the accessibility audit will run and call a test failure when
// accessibilityIssuesAreErrors(true) is called in the test function.
TEST_F('WebUIAccessibilityAuditBrowserTest_IssuesAreWarnings_ShouldFail',
       'testWithAuditFailuresAndIssuesAreErrors',
        function() {
  expectAuditWillRun(1);
  this.expectedWarnings = 1;
  this.expectedErrors = 2;

  this.accessibilityIssuesAreErrors = true;
  this.enableAccessibilityChecks();

  addAuditFailures();
});

// Tests that the accessibility audit will run twice if expectAccessibilityOk()
// is called during the test function and |runAccessibilityChecks| is true in
// the test fixture.
TEST_F('WebUIAccessibilityAuditBrowserTest_IssuesAreWarnings',
       'testWithAuditFailuresAndExpectA11yOk',
        function() {
  expectAuditWillRun(2);

  expectAccessibilityOk();

  this.expectedWarnings = 1;
  this.expectedErrors = 2;
  expectReportConsoleWarning();

  this.enableAccessibilityChecks();

  addAuditFailures();
});
