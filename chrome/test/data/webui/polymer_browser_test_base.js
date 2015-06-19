// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Framework for running JavaScript tests of Polymer elements.
 */

/**
 * Test fixture for Polymer element testing.
 * @constructor
 * @extends testing.Test
 */
function PolymerTest() {
}

PolymerTest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Navigate to a WebUI to satisfy BrowserTest conditions. Override to load a
   * more useful WebUI.
   * @override
   */
  browsePreload: 'chrome://chrome-urls/',

  /**
   * The mocha adapter assumes all tests are async.
   * @override
   * @final
   */
  isAsync: true,

  /**
   * Files that need not be compiled. Should be overridden to use correct
   * relative paths with PolymerTest.getLibraries.
   * @override
   */
  extraLibraries: [
    'ui/webui/resources/js/cr.js',
    'third_party/mocha/mocha.js',
    'chrome/test/data/webui/mocha_adapter.js',
  ],

  /** @override */
  setUp: function() {
    testing.Test.prototype.setUp.call(this);

    // Import Polymer and iron-test-helpers before running tests.
    suiteSetup(function() {
      return Promise.all([
        PolymerTest.importHtml(
            'chrome://resources/polymer/v1_0/polymer/polymer.html'),
        PolymerTest.importHtml(
            'chrome://resources/polymer/v1_0/iron-test-helpers/' +
            'iron-test-helpers.html'),
      ]);
    });
  },
};

/**
 * Imports the HTML file.
 * @param {string} src The URL to load.
 * @return {Promise} A promise that is resolved/rejected on success/failure.
 */
PolymerTest.importHtml = function(src) {
  var link = document.createElement('link');
  link.rel = 'import';
  var promise = new Promise(function(resolve, reject) {
    link.onload = resolve;
    link.onerror = reject;
  });
  link.href = src;
  document.head.appendChild(link);
  return promise;
};

/**
 * Removes all content from the body.
 */
PolymerTest.clearBody = function() {
  document.body.innerHTML = '';
};

/**
 * Helper function to return the list of extra libraries relative to basePath.
 */
PolymerTest.getLibraries = function(basePath) {
  // Ensure basePath ends in '/'.
  if (basePath.length && basePath[basePath.length - 1] != '/')
    basePath += '/';

  return PolymerTest.prototype.extraLibraries.map(function(library) {
    return basePath + library;
  });
};
