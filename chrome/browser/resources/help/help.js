// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../uber/uber_utils.js">

cr.define('help_page', function() {
  var localStrings = new LocalStrings();

  /**
   * Encapsulated handling of the help page.
   */
  function HelpPage() {}

  cr.addSingletonGetter(HelpPage);

  HelpPage.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Perform initial setup.
     */
    initialize: function() {
      uber.onContentFrameLoaded();

      // Set the title.
      var title = localStrings.getString('helpTitle');
      uber.invokeMethodOnParent('setTitle', {title: title});

      $('product-license').innerHTML = localStrings.getString('productLicense');

      var productTOS = $('product-tos');
      if (productTOS)
        productTOS.innerHTML = localStrings.getString('productTOS');

      if (!cr.isLinux) {
        $('relaunch').onclick = function() {
          chrome.send('relaunchNow');
        };
      }

      // Attempt to update.
      chrome.send('onPageLoaded');
    },

    /**
     * @private
     */
    setUpdateImage_: function(state) {
      $('update-status-icon').className = 'update-icon ' + state;
    },

    /**
     * @private
     */
    setUpdateStatus_: function(status) {
      if (status == 'checking') {
        this.setUpdateImage_('available');
        $('update-status').innerHTML =
            localStrings.getString('updateCheckStarted');
      } else if (status == 'updating') {
        this.setUpdateImage_('available');
        $('update-status').innerHTML = localStrings.getString('updating');
      } else if (status == 'nearly_updated') {
        this.setUpdateImage_('up-to-date');
        $('update-status').innerHTML =
            localStrings.getString('updateAlmostDone');
      } else if (status == 'updated') {
        this.setUpdateImage_('up-to-date');
        $('update-status').innerHTML = localStrings.getString('upToDate');
      }

      $('update-percentage').hidden = status != 'updating';
      $('relaunch').hidden = status != 'nearly_updated';
    },

    /**
     * @private
     */
    setProgress_: function(progress) {
      $('update-percentage').innerHTML = progress + "%";
    },

    /**
     * @private
     */
    setOSVersion_: function(version) {
      if (!cr.isChromeOS)
        console.error('OS version unsupported on non-CrOS');

      $('os-version').textContent = version;
    },

    /**
     * @private
     */
    setOSFirmware_: function(firmware) {
      if (!cr.isChromeOS)
        console.error('OS firmware unsupported on non-CrOS');

      $('firmware').textContent = firmware;
    },
  };

  HelpPage.setUpdateStatus = function(status) {
    HelpPage.getInstance().setUpdateStatus_(status);
  };

  HelpPage.setProgress = function(progress) {
    HelpPage.getInstance().setProgress_(progress);
  };

  HelpPage.setOSVersion = function(version) {
    HelpPage.getInstance().setOSVersion_(version);
  };

  HelpPage.setOSFirmware = function(firmware) {
    HelpPage.getInstance().setOSFirmware_(firmware);
  };

  // Export
  return {
    HelpPage: HelpPage
  };
});

var HelpPage = help_page.HelpPage;

window.onload = function() {
  HelpPage.getInstance().initialize();
};
