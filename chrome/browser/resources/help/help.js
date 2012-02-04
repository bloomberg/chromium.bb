// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
      chrome.send('checkForUpdate');
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
    }
  };

  HelpPage.setUpdateStatus = function(status) {
    HelpPage.getInstance().setUpdateStatus_(status);
  };

  HelpPage.setProgress = function(progress) {
    HelpPage.getInstance().setProgress_(progress);
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
