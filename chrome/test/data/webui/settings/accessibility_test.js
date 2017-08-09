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
 * The violation filter object maps individual audit rule IDs to functions that
 * return true for elements to filter from that rule's violations.
 * @typedef {Object<string, function(!axe.NodeResult): boolean>}
 */
AccessibilityTest.ViolationFilter;

/**
 * @typedef {{
 *   name: string,
 *   axeOptions: ?AccessibilityTest.AxeOptions,
 *   setup: ?function,
 *   tests: Object<string, function(): ?Promise>,
 *   violationFilter: ?AccessibilityTest.ViolationFilter
 * }}
 */
AccessibilityTest.Definition;

/**
 * Run aXe-core accessibility audit, print console-friendly representation
 * of violations to console, and fail the test.
 * @param {!AccessibilityTest.Definition} testDef Object configuring the audit.
 * @return {Promise} A promise that will be resolved with the accessibility
 *    audit is complete.
 */
AccessibilityTest.runAudit_ = function(testDef) {
  // Ignore iron-iconset-svg elements that have duplicate ids and result in
  // false postives from the audit.
  var context = {exclude: ['iron-iconset-svg']};
  options = testDef.axeOptions || {};
  // Element references needed for filtering audit results.
  options.elementRef = true;

  return new Promise((resolve, reject) => {
    axe.run(context, options, (err, results) => {
      if (err)
        reject(err);

      var filteredViolations = AccessibilityTest.filterViolations_(
          results.violations, testDef.violationFilter || {});

      var violationCount = filteredViolations.length;
      if (violationCount) {
        AccessibilityTest.print_(filteredViolations);
        reject('Found ' + violationCount + ' accessibility violations.');
      } else {
        resolve();
      }
    });
  });
};

/*
 * Get list of filtered audit violations.
 * @param {!Array<axe.Result>} violations List of accessibility violations.
 * @param {!AccessibilityTest.ViolationFilter} filter Object specifying set of
 *    violations to filter from the results.
 * @return {!Array<axe.Result>} List of filtered violations.
 */
AccessibilityTest.filterViolations_ = function(violations, filter) {
  if (Object.keys(filter).length == 0) {
    return violations;
  }

  var filteredViolations = [];
  // Check for and remove any nodes specified by filter.
  for (let violation of violations) {
    if (violation.id in filter) {
      var exclusionRule = filter[violation.id];
      violation.nodes = violation.nodes.filter(
          (node) => !exclusionRule(node));
    }

    if (violation.nodes.length > 0) {
      filteredViolations.push(violation);
    }
  }
  return filteredViolations;
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
      return promise.then(() => AccessibilityTest.runAudit_(testDef));
    } else {
      return AccessibilityTest.runAudit_(testDef);
    }
  };
};

/**
 * Remove circular references in |violations| and print violations to the
 * console.
 * @param {!Array<axe.Result>} List of violations to display
 */
AccessibilityTest.print_ = function(violations) {
  // Elements have circular references and must be removed before printing.
  for (let violation of violations) {
    for (let node of violation.nodes) {
      delete node['element'];
      ['all', 'any', 'none'].forEach((attribute) => {
        for (let checkResult of node[attribute]) {
          for (let relatedNode of checkResult.relatedNodes) {
            delete relatedNode['element'];
          }
        }
      });
    }
  }
  console.log(JSON.stringify(violations, null, 4));
};