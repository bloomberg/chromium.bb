// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for button navigation functionality in
 * MultiDevice setup WebUI.
 */
cr.define('multidevice_setup', () => {
  function registerTests() {
    suite('MultiDeviceSetup', () => {
      /**
       * MultiDeviceSetup created before each test.
       * @type {MultiDeviceSetup}
       */
      let multiDeviceSetupElement;

      const FAILURE = 'setup-failed-page';
      const SUCCESS = 'setup-succeeded-page';
      const START = 'start-setup-page';

      function tapForwardNavigation() {
        MockInteractions.tap(
            multiDeviceSetupElement.$$('button-bar').$.forward);
      }

      function tapBackwardNavigation() {
        MockInteractions.tap(
            multiDeviceSetupElement.$$('button-bar').$.backward);
      }

      setup(() => {
        PolymerTest.clearBody();
        multiDeviceSetupElement = document.createElement('multidevice-setup');
        document.body.appendChild(multiDeviceSetupElement);
      });

      test(
          'Check SetupFailedPage forward button goes to start page',
          done => {
            multiDeviceSetupElement.visiblePageName_ = FAILURE;
            multiDeviceSetupElement.addEventListener(
                'visible-page_-changed', function() {
                  if (multiDeviceSetupElement.visiblePage_ &&
                      multiDeviceSetupElement.visiblePage_.is == START) {
                    done();
                  }
                });
            tapForwardNavigation();
          });

      test('Check SetupFailedPage backward button closes UI', done => {
        multiDeviceSetupElement.visiblePageName_ = FAILURE;
        multiDeviceSetupElement.addEventListener('ui-closed', () => done());
        tapBackwardNavigation();
      });

      test('Check SetupSucceededPage forward button closes UI', done => {
        multiDeviceSetupElement.visiblePageName_ = SUCCESS;
        multiDeviceSetupElement.addEventListener('ui-closed', () => done());
        tapForwardNavigation();
      });

      test('Check StartSetupPage backward button closes UI', done => {
        multiDeviceSetupElement.visiblePageName_ = START;
        multiDeviceSetupElement.addEventListener('ui-closed', () => done());
        tapBackwardNavigation();
      });
    });
  }
  return {registerTests: registerTests};
});
