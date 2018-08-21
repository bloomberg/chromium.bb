// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of integration tests for MultiDevice setup WebUI. */
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

      const SUCCESS = 'setup-succeeded-page';
      const START = 'start-setup-page';

      setup(() => {
        multiDeviceSetupElement = document.createElement('multidevice-setup');
        multiDeviceSetupElement.multideviceSetup = new FakeMojoService();
        multiDeviceSetupElement.uiMode = multidevice_setup.UiMode.POST_OOBE;

        document.body.appendChild(multiDeviceSetupElement);
        forwardButton =
            multiDeviceSetupElement.$$('button-bar /deep/ #forward');
        backwardButton =
            multiDeviceSetupElement.$$('button-bar /deep/ #backward');
      });

      /** @param {string} visiblePageName */
      function setVisiblePage(visiblePageName) {
        multiDeviceSetupElement.visiblePageName_ = visiblePageName;
        Polymer.dom.flush();
      }

      // From SetupSucceededPage

      test('SetupSucceededPage forward button closes UI', done => {
        setVisiblePage(SUCCESS);
        multiDeviceSetupElement.addEventListener('setup-exited', () => done());
        forwardButton.click();
      });

      // From StartSetupPage

      // OOBE

      test('StartSetupPage backward button continues OOBE (OOBE)', done => {
        multiDeviceSetupElement.addEventListener('setup-exited', () => {
          done();
        });

        setVisiblePage(START);
        multiDeviceSetupElement.multideviceSetup.shouldSetHostSucceed = true;
        multiDeviceSetupElement.uiMode = multidevice_setup.UiMode.OOBE;

        backwardButton.click();
      });

      test(
          'StartSetupPage forward button sets host in backround and ' +
              'continues OOBE (OOBE).',
          done => {
            multiDeviceSetupElement.addEventListener('setup-exited', () => {
              done();
            });

            setVisiblePage(START);
            multiDeviceSetupElement.multideviceSetup.shouldSetHostSucceed =
                true;
            multiDeviceSetupElement.uiMode = multidevice_setup.UiMode.OOBE;

            forwardButton.click();
          });

      // Post-OOBE

      test('StartSetupPage backward button closes UI (post-OOBE)', done => {
        multiDeviceSetupElement.addEventListener('setup-exited', () => done());

        setVisiblePage(START);
        multiDeviceSetupElement.multideviceSetup.shouldSetHostSucceed = true;
        multiDeviceSetupElement.uiMode = multidevice_setup.UiMode.POST_OOBE;

        backwardButton.click();
      });

      test(
          'StartSetupPage forward button goes to success page if mojo works ' +
              '(post-OOBE)',
          done => {
            multiDeviceSetupElement.addEventListener(
                'visible-page-name_-changed', () => {
                  if (multiDeviceSetupElement.$$('iron-pages > .iron-selected')
                          .is == SUCCESS)
                    done();
                });

            setVisiblePage(START);
            multiDeviceSetupElement.multideviceSetup.shouldSetHostSucceed =
                true;
            multiDeviceSetupElement.uiMode = multidevice_setup.UiMode.POST_OOBE;

            forwardButton.click();
          });
    });
  }
  return {registerIntegrationTests: registerIntegrationTests};
});
