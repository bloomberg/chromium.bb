// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of integration tests for MultiDevice setup WebUI. */
cr.define('multidevice_setup', () => {
  /** @implements {multidevice_setup.MultiDeviceSetupDelegate} */
  class FakeDelegate {
    constructor() {
      /** @private {boolean} */
      this.isPasswordRequiredToSetHost_ = true;

      /** @private {boolean} */
      this.shouldSetHostSucceed_ = true;
    }

    set isPasswordRequired(isPasswordRequired) {
      this.isPasswordRequiredToSetHost_ = isPasswordRequired;
    }

    /** @override */
    isPasswordRequiredToSetHost() {
      return this.isPasswordRequiredToSetHost_;
    }

    set shouldSetHostSucceed(shouldSetHostSucceed) {
      this.shouldSetHostSucceed_ = shouldSetHostSucceed;
    }

    /** @override */
    setHostDevice(hostDeviceId, opt_authToken) {
      return new Promise((resolve) => {
        resolve({success: this.shouldSetHostSucceed_});
      });
    }

    set shouldExitSetupFlowAfterHostSet(shouldExitSetupFlowAfterHostSet) {
      this.shouldExitSetupFlowAfterSettingHost_ =
          shouldExitSetupFlowAfterHostSet;
    }

    /** @override */
    shouldExitSetupFlowAfterSettingHost() {
      return this.shouldExitSetupFlowAfterSettingHost_;
    }
  }

  /** @implements {multidevice_setup.MojoInterfaceProvider} */
  class FakeMojoInterfaceProviderImpl {
    /** @param {!FakeMojoService} fakeMojoService */
    constructor(fakeMojoService) {
      /** @private {!FakeMojoService} */
      this.fakeMojoService_ = fakeMojoService;
    }

    /** @override */
    getInterfacePtr() {
      return this.fakeMojoService_;
    }
  }

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

      /** @type {!FakeMojoService} */
      let fakeMojoService;

      const PASSWORD = 'password-page';
      const SUCCESS = 'setup-succeeded-page';
      const START = 'start-setup-page';

      setup(() => {
        multiDeviceSetupElement = document.createElement('multidevice-setup');
        multiDeviceSetupElement.delegate = new FakeDelegate();
        fakeMojoService = new FakeMojoService();
        multiDeviceSetupElement.mojoInterfaceProvider_ =
            new FakeMojoInterfaceProviderImpl(fakeMojoService);

        document.body.appendChild(multiDeviceSetupElement);
        forwardButton = multiDeviceSetupElement.$$('button-bar').$$('#forward');
        backwardButton =
            multiDeviceSetupElement.$$('button-bar').$$('#backward');
      });

      /** @param {boolean} isOobeMode */
      function setMode(isOobeMode) {
        multiDeviceSetupElement.delegate.isPasswordRequired = !isOobeMode;
        multiDeviceSetupElement.delegate.shouldExitSetupFlowAfterHostSet =
            isOobeMode;
      }

      /** @param {string} visiblePageName */
      function setVisiblePage(visiblePageName) {
        multiDeviceSetupElement.visiblePageName_ = visiblePageName;
        Polymer.dom.flush();
      }

      // From SetupSucceededPage

      test('SetupSucceededPage forward button closes UI', done => {
        setVisiblePage(SUCCESS);
        multiDeviceSetupElement.addEventListener('setup-exited', () => done());

        multiDeviceSetupElement.async(() => {
          forwardButton.click();
        });
      });

      // From StartSetupPage

      // OOBE

      test('StartSetupPage backward button continues OOBE (OOBE)', done => {
        multiDeviceSetupElement.addEventListener('setup-exited', () => {
          done();
        });

        setMode(true /* isOobeMode */);
        setVisiblePage(START);
        multiDeviceSetupElement.delegate.shouldSetHostSucceed = true;

        backwardButton.click();
      });

      test(
          'StartSetupPage forward button sets host in background and ' +
              'goes to PasswordPage (OOBE).',
          done => {
            multiDeviceSetupElement.addEventListener('setup-exited', () => {
              done();
            });

            setMode(true /* isOobeMode */);
            setVisiblePage(START);
            multiDeviceSetupElement.delegate.shouldSetHostSucceed = true;

            multiDeviceSetupElement.async(() => {
              forwardButton.click();
            });
          });

      // Post-OOBE

      test('StartSetupPage backward button closes UI (post-OOBE)', done => {
        multiDeviceSetupElement.addEventListener('setup-exited', () => done());

        setMode(false /* isOobeMode */);
        setVisiblePage(START);
        multiDeviceSetupElement.delegate.shouldSetHostSucceed = true;

        backwardButton.click();
      });

      test('PasswordPage backward button closes UI (post-OOBE)', done => {
        multiDeviceSetupElement.addEventListener('setup-exited', () => done());

        setMode(false /* isOobeMode */);
        setVisiblePage(PASSWORD);
        multiDeviceSetupElement.delegate.shouldSetHostSucceed = true;

        backwardButton.click();
      });

      test(
          'PasswordPage forward button goes to success page if mojo works ' +
              '(post-OOBE)',
          done => {
            multiDeviceSetupElement.addEventListener(
                'visible-page-name_-changed', () => {
                  if (multiDeviceSetupElement.visiblePageName_ == SUCCESS)
                    done();
                });

            setMode(false /* isOobeMode */);
            setVisiblePage(PASSWORD);
            multiDeviceSetupElement.delegate.shouldSetHostSucceed = true;

            multiDeviceSetupElement.async(() => {
              forwardButton.click();
            });
          });

      test('SuccessPage Settings link closes UI (post-OOBE)', done => {
        multiDeviceSetupElement.addEventListener('setup-exited', () => done());

        setMode(false /* isOobeMode */);
        setVisiblePage(SUCCESS);

        multiDeviceSetupElement.$$('setup-succeeded-page')
            .$$('#settings-link')
            .click();
      });
    });
  }
  return {registerIntegrationTests: registerIntegrationTests};
});
