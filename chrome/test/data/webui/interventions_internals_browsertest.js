// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for interventions_internals.js
 */

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../';

/**
 * Test fixture for InterventionsInternals WebUI testing.
 * @constructor
 * @extends testing.Test
 */
function InterventionsInternalsUITest() {
  this.setupFnResolver = new PromiseResolver();
}

InterventionsInternalsUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the options page and call preLoad().
   * @override
   */
  browsePreload: 'chrome://interventions-internals',

  /** @override */
  isAsync: true,

  extraLibraries: [
    ROOT_PATH + 'third_party/mocha/mocha.js',
    ROOT_PATH + 'chrome/test/data/webui/mocha_adapter.js',
    ROOT_PATH + 'ui/webui/resources/js/promise_resolver.js',
    ROOT_PATH + 'ui/webui/resources/js/util.js',
    ROOT_PATH + 'chrome/test/data/webui/test_browser_proxy.js',
  ],

  preLoad: function() {
    /**
     * A stub class for the Mojo PageHandler.
     */
    class TestPageHandler extends TestBrowserProxy {
      constructor() {
        super(['getPreviewsEnabled']);

        /** @private {boolean} */
        this.previewsEnabled_ = false;
      }

      /**
       * Change the behavior of the PageHandler for testing purposes.
       * @param {boolean} The new status for previews.
       */
      setPreviewsEnabled(enabled) {
        this.previewsEnabled_ = enabled;
      }

      /** @override */
      getPreviewsEnabled() {
        this.methodCalled('getPreviewsEnabled');
        return Promise.resolve({
          enabled: this.previewsEnabled_,
        });
      }
    }

    window.setupFn = function() {
      return this.setupFnResolver.promise;
    }.bind(this);

    window.testPageHandler = new TestPageHandler();
  },
};

TEST_F('InterventionsInternalsUITest', 'PreviewStatusEnabledTest', function() {
  let setupFnResolver = this.setupFnResolver;

  test('EnabledStatusTest', function() {
    // Setup testPageHandler behavior.
    window.testPageHandler.setPreviewsEnabled(true);
    setupFnResolver.resolve();

    return setupFnResolver.promise
        .then(function() {
          return window.testPageHandler.whenCalled('getPreviewsEnabled');
        })
        .then(function() {
          let message = document.querySelector('#offlinePreviews');
          expectEquals('OfflinePreviews: Enabled', message.textContent);
        });
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'PreviewStatusDisabledTest', function() {
  let setupFnResolver = this.setupFnResolver;

  test('DisabledStatusTest', function() {
    // Setup testPageHandler behavior.
    window.testPageHandler.setPreviewsEnabled(false);
    setupFnResolver.resolve();

    return setupFnResolver.promise
        .then(function() {
          return window.testPageHandler.whenCalled('getPreviewsEnabled');
        })
        .then(function() {
          let message = document.querySelector('#offlinePreviews');
          expectEquals('OfflinePreviews: Disabled', message.textContent);
        });
  });

  mocha.run();
});
