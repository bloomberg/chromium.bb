// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-load-error. */
cr.define('extension_load_error_tests', function() {
  /** @enum {string} */
  var TestNames = {
    Interaction: 'Interaction',
    CodeSection: 'Code Section',
  };

  /**
   * @implements {extensions.LoadErrorDelegate}
   * @extends {extension_test_util.ClickMock}
   * @constructor
   */
  function MockDelegate() {}

  MockDelegate.prototype = {
    __proto__: extension_test_util.ClickMock.prototype,

    /** @override */
    retryLoadUnpacked: function() {},
  };

  function registerTests() {
    suite('ExtensionLoadErrorTests', function() {
      /** @type {extensions.LoadError} */
      var loadError;

      /** @type {MockDelegate} */
      var mockDelegate;

      setup(function() {
        PolymerTest.clearBody();
        mockDelegate = new MockDelegate();
        loadError = new extensions.LoadError();
        loadError.delegate = mockDelegate;
        document.body.appendChild(loadError);
      });

      test(assert(TestNames.Interaction), function() {
        var dialogElement = loadError.$$('dialog');
        var isDialogVisible = function() {
          var rect = dialogElement.getBoundingClientRect();
          return rect.width * rect.height > 0;
        };

        expectFalse(isDialogVisible());
        loadError.show();
        expectTrue(isDialogVisible());

        mockDelegate.testClickingCalls(
            loadError.$$('#retry'), 'retryLoadUnpacked', []);
        expectFalse(isDialogVisible());

        loadError.show();
        MockInteractions.tap(loadError.$$('#dismiss'));
        expectFalse(isDialogVisible());
      });

      test(assert(TestNames.CodeSection), function() {
        expectTrue(loadError.$.code.isEmpty());
        var loadErrorProperties = {
          error: 'Some error',
          path: '/some/path',
          source: {
            beforeHighlight: 'before',
            highlight: 'highlight',
            afterHighlight: 'after',
          },
        };

        loadError.set('loadError', loadErrorProperties);
        expectFalse(loadError.$.code.isEmpty());
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
