// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for button navigation functionality in
 * MultiDevice setup WebUI.
 */
cr.define('multidevice_setup', () => {
  function registerIntegrationTests() {
    suite('MultiDeviceSetup', () => {
      /**
       * MultiDeviceSetup created before each test. Defined in setUp.
       * @type {MultiDeviceSetup|undefined}
       */
      let multiDeviceSetupElement;

      /**
       * Forward navigation button. Defined in setUp.
       * @type {PaperButton|undefined}
       */
      let forwardButton;

      /**
       * Backward navigation button. Defined in setUp.
       * @type {PaperButton|undefined}
       */
      let backwardButton;

      const FAILURE = 'setup-failed-page';
      const SUCCESS = 'setup-succeeded-page';
      const START = 'start-setup-page';

      setup(() => {
        multiDeviceSetupElement = document.createElement('multidevice-setup');
        document.body.appendChild(multiDeviceSetupElement);
        forwardButton =
            multiDeviceSetupElement.$$('button-bar /deep/ #forward');
        backwardButton =
            multiDeviceSetupElement.$$('button-bar /deep/ #backward');
      });

      test('SetupFailedPage forward button goes to start page', done => {
        multiDeviceSetupElement.addEventListener(
            'visible-page_-changed', function() {
              if (multiDeviceSetupElement.$$('iron-pages > .iron-selected')
                      .is == START) {
                done();
              }
            });
        multiDeviceSetupElement.visiblePageName_ = FAILURE;
        MockInteractions.tap(forwardButton);
      });

      test('SetupFailedPage backward button closes UI', done => {
        multiDeviceSetupElement.addEventListener('ui-closed', () => done());
        multiDeviceSetupElement.visiblePageName_ = FAILURE;
        MockInteractions.tap(backwardButton);
      });

      test('SetupSucceededPage forward button closes UI', done => {
        multiDeviceSetupElement.visiblePageName_ = SUCCESS;
        multiDeviceSetupElement.addEventListener('ui-closed', () => done());
        MockInteractions.tap(forwardButton);
      });

      test('StartSetupPage backward button closes UI', done => {
        multiDeviceSetupElement.visiblePageName_ = START;
        multiDeviceSetupElement.addEventListener('ui-closed', () => done());
        MockInteractions.tap(backwardButton);
      });
    });
  }
  return {registerIntegrationTests: registerIntegrationTests};
});
