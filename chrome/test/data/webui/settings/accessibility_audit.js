// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Accessibility Test API
 */

/**
 * Accessibility Test
 * @namespace
 */
var AccessibilityTest = AccessibilityTest || {};

/**
 * @typedef {{
 *   runOnly: {
 *     type: string,
 *     values: Array<string>
 *   }
 * }}
 * @see https://github.com/dequelabs/axe-core/blob/develop/doc/API.md#options-parameter
 */
AccessibilityTest.AxeOptions;

/**
 * @typedef {{
 *   name: string,
 *   axeOptions: ?AccessibilityTest.AxeOptions,
 *   setup: ?function,
 *   tests: Object<string, function(): ?Promise>
 * }}
 */
AccessibilityTest.Definition;

/**
 * Run aXe-core accessibility audit, print console-friendly representation
 * of violations to console, and fail the test.
 * @param {AccessibilityTest.AxeOptions} options Dictionary disabling specific
 *    audit rules.
 * @return {Promise} A promise that will be resolved with the accessibility
 *    audit is complete.
 */
AccessibilityTest.runAudit_ = function(options) {
  // Ignore iron-iconset-svg elements that have duplicate ids and result in
  // false postives from the audit.
  var context = {exclude: ['iron-iconset-svg']};
  options = options || {};

  return new Promise((resolve, reject) => {
    axe.run(context, options, (err, results) => {
      if (err)
        reject(err);

      var violationCount = results.violations.length;
      if (violationCount) {
        // Pretty print out the violations detected by the audit.
        console.log(JSON.stringify(results.violations, null, 4));
        reject('Found ' + violationCount + ' accessibility violations.');
      } else {
        resolve();
      }
    });
  });
};

/**
 * Define a different test for each audit rule, unless overridden by
 * |test.options.runOnly|.
 * @param {AccessibilityTestDefinition} test Object configuring the test.
 * @constructor
 */
AccessibilityTest.define = function(test) {
  var axeOptions = test.axeOptions || {};
  // Maintain a list of tests to define.
  var tests = [test];

  // This option override should go first b/c it creates multiple test targets.
  if (!axeOptions.runOnly) {
    tests = [];
    // Define a test for each audit rule separately.
    for (let ruleId of AccessibilityTest.ruleIds) {
      var newTestDefinition = Object.assign({}, test);
      newTestDefinition.name += '_' + ruleId;
      newTestDefinition.axeOptions = Object.assign({}, axeOptions);
      newTestDefinition.axeOptions.runOnly = {type: 'rule', values: [ruleId]};

      tests.push(newTestDefinition)
    }
  }

  // Define the mocha tests.
  AccessibilityTest.defineAccessibilityTestSuite_(tests);
}

/**
 * Define mocha suite(s) testing accessibility for each test definition in
 * given list.
 * @param {Array<AccessibilityTestDefinition>} List of test definitions.
 */
AccessibilityTest.defineAccessibilityTestSuite_ = function(tests) {
  for (let testDef of tests) {
    suite(testDef.name, () => {
      setup(testDef.setup.bind(testDef));
      for (var testMember in testDef.tests) {
        test(testMember, AccessibilityTest.getMochaTest_(testMember, testDef));
      }
    });
  }
}

/**
 *
 * Return a function that runs the accessibility audit after executing
 * the function corresponding to the |testDef.tests.testMember|.
 * @param {string} testMember The name of the mocha test
 * @param {AccessibilityTestDefinition} testDef Object configuring the test
 *    suite to which this test belongs.
 */
AccessibilityTest.getMochaTest_ = function(testMember, testDef) {
  return () => {
    // Run commands specified by the test definition followed by the
    // accessibility audit.
    var promise = testDef.tests[testMember].call(testDef);
    if (promise) {
      return promise.then(
          () => AccessibilityTest.runAudit_(testDef.axeOptions));
    } else {
      return AccessibilityTest.runAudit_(testDef.axeOptions);
    }
  };
};
