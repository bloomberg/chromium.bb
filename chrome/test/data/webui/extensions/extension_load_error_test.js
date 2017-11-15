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

  var suiteName = 'ExtensionLoadErrorTests';

  suite(suiteName, function() {
    /** @type {extensions.LoadError} */
    var loadError;

    /** @type {MockDelegate} */
    var mockDelegate;

    var fakeGuid = 'uniqueId';

    var stubLoadError = {
      error: 'error',
      path: 'some/path/',
      retryGuid: fakeGuid,
    };

    setup(function() {
      PolymerTest.clearBody();
      mockDelegate = new extensions.TestService();
      loadError = new extensions.LoadError();
      loadError.delegate = mockDelegate;
      loadError.loadError = stubLoadError;
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

      MockInteractions.tap(loadError.$$('.action-button'));
      return mockDelegate.whenCalled('retryLoadUnpacked').then(arg => {
        expectEquals(fakeGuid, arg);
        expectFalse(isDialogVisible());

        loadError.show();
        MockInteractions.tap(loadError.$$('.cancel-button'));
        expectFalse(isDialogVisible());
      });
    });

    test(assert(TestNames.CodeSection), function() {
      expectTrue(loadError.$.code.isEmpty());
      var loadErrorWithSource = {
        error: 'Some error',
        path: '/some/path',
        source: {
          beforeHighlight: 'before',
          highlight: 'highlight',
          afterHighlight: 'after',
        },
      };

      loadError.loadError = loadErrorWithSource;
      expectFalse(loadError.$.code.isEmpty());
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
