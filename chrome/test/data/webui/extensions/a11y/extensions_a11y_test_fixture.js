// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Accessibility Extensions tests. */

/** @const {string} Path to root from chrome/test/data/webui/extensions/a11y. */
const ROOT_PATH = '../../../../../../';

// Polymer BrowserTest fixture and aXe-core accessibility audit.
GEN_INCLUDE([
  ROOT_PATH + 'chrome/test/data/webui/a11y/accessibility_test.js',
  ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js',
]);
GEN('#include "chrome/common/chrome_features.h"');

/**
 * Test fixture for Accessibility of Chrome Extensions.
 * @constructor
 * @extends {PolymerTest}
 */
var ExtensionsA11yTestFixture = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/';
  }

  /** @override */
  get featureList() {
    return ['features::kMaterialDesignExtensions', ''];
  }

  // Include files that define the mocha tests.
  get extraLibraries() {
    return PolymerTest.getLibraries(ROOT_PATH);
  }

  // Default accessibility audit options. Specify in test definition to use.
  static get axeOptions() {
    return {
      'rules': {
        // Disable 'skip-link' check since there are few tab stops before the
        // main content.
        'skip-link': {enabled: false},
        // TODO(crbug.com/761461): enable after addressing flaky tests.
        'color-contrast': {enabled: false},
      },
    };
  }

  // Default accessibility violation filter. Specify in test definition to use.
  static get violationFilter() {
    return {
      // Different iron-iconset-svg can have children with the same id.
      'duplicate-id': function(nodeResult) {
        // Only safe to ignore if ALL dupe ids are children of iron-iconset-svg.
        return nodeResult.any.every((hit) => {
          return hit.relatedNodes.every(node => {
            return ExtensionsA11yTestFixture.hasAncestor_(
                node.element, 'iron-iconset-svg');
          });
        });
      }
    };
  }

  /**
   * @param {!HTMLNode} node
   * @param {string} type
   * @return {boolean} Whether any ancestor of |node| is a |type| element.
   * @private
   */
  static hasAncestor_(node, type) {
    if (!node.parentElement)
      return false;

    return (node.parentElement.tagName.toLocaleLowerCase() == type) ||
        ExtensionsA11yTestFixture.hasAncestor_(node.parentElement, type);
  }
};
