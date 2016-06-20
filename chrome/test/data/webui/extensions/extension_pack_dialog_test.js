// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-pack-dialog. */
cr.define('extension_pack_dialog_tests', function() {
  /** @enum {string} */
  var TestNames = {
    Interaction: 'Interaction',
  };

  /**
   * @implements {extensions.PackDialogDelegate}
   * @constructor
   */
  function MockDelegate() {}

  MockDelegate.prototype = {
    /** @override */
    choosePackRootDirectory: function() {
      this.rootPromise = new PromiseResolver();
      return this.rootPromise.promise;
    },

    /** @override */
    choosePrivateKeyPath: function() {
      this.keyPromise = new PromiseResolver();
      return this.keyPromise.promise;
    },

    /** @override */
    packExtension: function(rootPath, keyPath) {
      this.rootPath = rootPath;
      this.keyPath = keyPath;
    },
  };

  function registerTests() {
    suite('ExtensionPackDialogTests', function() {
      /** @type {extensions.PackDialog} */
      var packDialog;

      /** @type {MockDelegate} */
      var mockDelegate;

      setup(function() {
        PolymerTest.clearBody();
        mockDelegate = new MockDelegate();
        packDialog = new extensions.PackDialog();
        packDialog.delegate = mockDelegate;
        document.body.appendChild(packDialog);
      });

      test(assert(TestNames.Interaction), function() {
        var dialogElement = packDialog.$$('dialog');
        var isDialogVisible = function() {
          var rect = dialogElement.getBoundingClientRect();
          return rect.width * rect.height > 0;
        };

        expectFalse(isDialogVisible());
        packDialog.show();
        expectTrue(isDialogVisible());
        expectEquals('', packDialog.$$('#root-dir').value);
        MockInteractions.tap(packDialog.$$('#root-dir-browse'));
        expectTrue(!!mockDelegate.rootPromise);
        expectEquals('', packDialog.$$('#root-dir').value);
        var kRootPath = 'this/is/a/path';

        var promises = [];
        promises.push(mockDelegate.rootPromise.promise.then(function() {
          expectEquals(kRootPath, packDialog.$$('#root-dir').value);
          expectEquals(kRootPath, packDialog.packDirectory_);
        }));

        expectEquals('', packDialog.$$('#key-file').value);
        MockInteractions.tap(packDialog.$$('#key-file-browse'));
        expectTrue(!!mockDelegate.keyPromise);
        expectEquals('', packDialog.$$('#key-file').value);
        var kKeyPath = 'here/is/another/path';

        promises.push(mockDelegate.keyPromise.promise.then(function() {
          expectEquals(kKeyPath, packDialog.$$('#key-file').value);
          expectEquals(kKeyPath, packDialog.keyFile_);
        }));

        mockDelegate.rootPromise.resolve(kRootPath);
        mockDelegate.keyPromise.resolve(kKeyPath);

        return Promise.all(promises).then(function() {
          MockInteractions.tap(packDialog.$$('#confirm'));
          expectEquals(kRootPath, mockDelegate.rootPath);
          expectEquals(kKeyPath, mockDelegate.keyPath);
          expectFalse(isDialogVisible());
        });
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
