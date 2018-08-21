// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('multidevice_setup');

  /** @enum {string} */
multidevice_setup.PageName = {
  FAILURE: 'setup-failed-page',
  SUCCESS: 'setup-succeeded-page',
  START: 'start-setup-page',
};

cr.define('multidevice_setup', function() {
  const PageName = multidevice_setup.PageName;

  const MultiDeviceSetup = Polymer({
    is: 'multidevice-setup',

    properties: {
      /**
       * Indicates whether UI was opened during OOBE flow or afterward.
       *
       * @type {!multidevice_setup.UiMode}
       */
      uiMode: Number,

      /**
       * Element name of the currently visible page.
       *
       * @private {!multidevice_setup.PageName}
       */
      visiblePageName_: {
        type: String,
        value: PageName.START,
        // For testing purporses only
        notify: true,
      },

      /**
       * DOM Element corresponding to the visible page.
       *
       * @private {!StartSetupPageElement|!SetupSucceededPageElement|
       *           !SetupFailedPageElement}
       */
      visiblePage_: Object,

      /**
       * Array of objects representing all potential MultiDevice hosts.
       *
       * @private {!Array<!chromeos.deviceSync.mojom.RemoteDevice>}
       */
      devices_: Array,

      /**
       * Unique identifier for the currently selected host device.
       *
       * Undefined if the no list of potential hosts has been received from mojo
       * service.
       *
       * @private {string|undefined}
       */
      selectedDeviceId_: String,
    },

    behaviors: [
      MojoApiBehavior,
    ],

    listeners: {
      'backward-navigation-requested': 'onBackwardNavigationRequested_',
      'forward-navigation-requested': 'onForwardNavigationRequested_',
    },

    /** @override */
    attached: function() {
      this.multideviceSetup.getEligibleHostDevices()
          .then((responseParams) => {
            if (responseParams.eligibleHostDevices.length == 0) {
              console.warn('Potential host list is empty.');
              return;
            }

            this.devices_ = responseParams.eligibleHostDevices;
          })
          .catch((error) => {
            console.warn('Mojo service failure: ' + error);
          });
    },

    /** @private */
    onBackwardNavigationRequested_: function() {
      this.exitSetupFlow_();
    },

    /** @private */
    onForwardNavigationRequested_: function() {
      this.visiblePage_.getCanNavigateToNextPage().then((canNavigate) => {
        if (!canNavigate)
          return;
        this.navigateForward_();
      });
    },

    /** @private */
    navigateForward_: function() {
      switch (this.visiblePageName_) {
        case PageName.FAILURE:
          this.visiblePageName_ = PageName.START;
          return;
        case PageName.SUCCESS:
          this.exitSetupFlow_();
          return;
        case PageName.START:
          let deviceId = /** @type {string} */ (this.selectedDeviceId_);
          this.multideviceSetup.setHostDevice(deviceId)
              .then((responseParams) => {
                if (!responseParams.success) {
                  console.warn(
                      'Failure setting device with device ID: ' +
                      this.selectedDeviceId_);
                  return;
                }

                switch (this.uiMode) {
                  case multidevice_setup.UiMode.OOBE:
                    this.exitSetupFlow_();
                    return;
                  case multidevice_setup.UiMode.POST_OOBE:
                    this.visiblePageName_ = PageName.SUCCESS;
                    return;
                }
              })
              .catch((error) => {
                console.warn('Mojo service failure: ' + error);
              });
      }
    },

    /**
     * Notifies observers that the setup flow has completed.
     *
     * @private
     */
    exitSetupFlow_: function() {
      console.log('Exiting Setup Flow');
      this.fire('setup-exited');
    },
  });

  return {
    MultiDeviceSetup: MultiDeviceSetup,
  };
});
