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

    // List of imported URLs for debugging purposes.
    PolymerTest.importUrls_ = [];

    // Importing a URL like "chrome://md-settings/foo" redirects to the base
    // ("chrome://md-settings") page, which due to how browsePreload works can
    // result in duplicate imports. Wrap document.registerElement so failures
    // caused by re-registering Polymer elements are caught; otherwise Chrome
    // simply throws "Script error" which is unhelpful.
    var originalRegisterElement = document.registerElement;
    document.registerElement = function() {
      try {
        return originalRegisterElement.apply(document, arguments);
      } catch (e) {
        var msg =
            'If the call to document.registerElement failed because a type ' +
            'is already registered, perhaps you have loaded a script twice. ' +
            'Incorrect resource URLs can redirect to base WebUI pages; make ' +
            'sure the following URLs are correct and unique:\n';
        for (var i = 0; i < PolymerTest.importUrls_.length; i++)
          msg += '  ' + PolymerTest.importUrls_[i] + '\n';
        console.error(msg);

        // Mocha will handle the error.
        throw e;
      }
    };

    // Import Polymer and iron-test-helpers before running tests.
    suiteSetup(function() {
      var promises = [];
      if (typeof Polymer != 'function') {
        promises.push(
            PolymerTest.importHtml(
                'chrome://resources/polymer/v1_0/polymer/polymer.html'));
      }
      if (typeof TestHelpers != 'object') {
        promises.push(
            PolymerTest.importHtml(
                'chrome://resources/polymer/v1_0/iron-test-helpers/' +
                'iron-test-helpers.html'));
      }
      return Promise.all(promises);
    });
  },
};

/**
 * Imports the HTML file.
 * @param {string} src The URL to load.
 * @return {Promise} A promise that is resolved/rejected on success/failure.
 */
PolymerTest.importHtml = function(src) {
  PolymerTest.importUrls_.push(src);
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
