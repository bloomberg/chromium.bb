// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../uber/uber_utils.js">
<include src="more_info_overlay.js">

cr.define('help', function() {
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

      var moreInfoOverlay = help.MoreInfoOverlay.getInstance();
      moreInfoOverlay.initializePage();

      var self = this;
      var moreInfo = $('more-info');
      if (moreInfo) {
        moreInfo.onclick = function() {
          self.showOverlay_($('more-info-overlay'));
        };
      }

      var reportIssue = $('report-issue');
      if (reportIssue)
        reportIssue.onclick = this.reportAnIssue_.bind(this);

      var promote = $('promote');
      if (promote) {
        promote.onclick = function() {
          chrome.send('promoteUpdater');
        };
      }

      var relaunch = $('relaunch');
      if (relaunch) {
        relaunch.onclick = function() {
          chrome.send('relaunchNow');
        };
      }

      // Attempt to update.
      chrome.send('onPageLoaded');
    },

    /**
     * @private
     */
    reportAnIssue_: function() {
      chrome.send('openFeedbackDialog');
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
    setUpdateStatus_: function(status, message) {
      if (status == 'checking') {
        this.setUpdateImage_('working');
        $('update-status').innerHTML =
            localStrings.getString('updateCheckStarted');
      } else if (status == 'updating') {
        this.setUpdateImage_('working');
        $('update-status').innerHTML = localStrings.getString('updating');
      } else if (status == 'nearly_updated') {
        this.setUpdateImage_('up-to-date');
        $('update-status').innerHTML =
            localStrings.getString('updateAlmostDone');
      } else if (status == 'updated') {
        this.setUpdateImage_('up-to-date');
        $('update-status').innerHTML = localStrings.getString('upToDate');
      } else if (status == 'failed') {
        this.setUpdateImage_('failed');
        $('update-status').innerHTML = message;
      } else if (status == 'disabled') {
        var container = $('update-status-container');
        if (container)
          container.hidden = true;
        return;
      }

      if (!cr.isMac)
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
    setPromotionState_: function(state) {
      if (state == 'hidden') {
        $('promote').hidden = true;
      } else if (state == 'enabled') {
        $('promote').disabled = false;
        $('promote').hidden = false;
      } else if (state == 'disabled') {
        $('promote').disabled = true;
        $('promote').hidden = false;
      }
    },

    /**
     * @private
     */
    setOSVersion_: function(version) {
      if (!cr.isChromeOS)
        console.error('OS version unsupported on non-CrOS');

      $('os-version').parentNode.hidden = (version == '');
      $('os-version').textContent = version;
    },

    /**
     * @private
     */
    setOSFirmware_: function(firmware) {
      if (!cr.isChromeOS)
        console.error('OS firmware unsupported on non-CrOS');

      $('firmware').parentNode.hidden = (firmware == '');
      $('firmware').textContent = firmware;
    },

    /**
     * Sets the given overlay to show. This hides whatever overlay is currently
     * showing, if any.
     * @param {HTMLElement} node The overlay page to show. If null, all
     *     overlays are hidden.
     */
    showOverlay_: function(node) {
      var currentlyShowingOverlay =
        document.querySelector('#overlay .page.showing');
      if (currentlyShowingOverlay)
        currentlyShowingOverlay.classList.remove('showing');

      if (node)
        node.classList.add('showing');
      $('overlay').hidden = !node;
    },

    /**
     * |enabled| is true if the release channel can be enabled.
     * @private
     */
    updateEnableReleaseChannel_: function(enabled) {
      $('more-info').hidden = !enabled;
    },

    /**
     * @private
     */
    updateSelectedChannel_: function(value) {
      var options = $('channel-changer').querySelectorAll('option');
      for (var i = 0; i < options.length; i++) {
        var option = options[i];
        if (option.value == value)
          option.selected = true;
      }
    },

    /**
     * @private
     */
    setReleaseChannel_: function(channel) {
      chrome.send('setReleaseTrack', [channel]);
    },
  };

  HelpPage.setUpdateStatus = function(status, message) {
    HelpPage.getInstance().setUpdateStatus_(status, message);
  };

  HelpPage.setProgress = function(progress) {
    HelpPage.getInstance().setProgress_(progress);
  };

  HelpPage.setPromotionState = function(state) {
    HelpPage.getInstance().setPromotionState_(state);
  };

  HelpPage.setOSVersion = function(version) {
    HelpPage.getInstance().setOSVersion_(version);
  };

  HelpPage.setOSFirmware = function(firmware) {
    HelpPage.getInstance().setOSFirmware_(firmware);
  };

  HelpPage.showOverlay = function(node) {
    HelpPage.getInstance().showOverlay_(node);
  };

  HelpPage.updateSelectedChannel = function(channel) {
    HelpPage.getInstance().updateSelectedChannel_(channel);
  };

  HelpPage.updateEnableReleaseChannel = function(enabled) {
    HelpPage.getInstance().updateEnableReleaseChannel_(enabled);
  };

  HelpPage.setReleaseChannel = function(channel) {
    HelpPage.getInstance().setReleaseChannel_(channel);
  };

  // Export
  return {
    HelpPage: HelpPage
  };
});

window.onload = function() {
  help.HelpPage.getInstance().initialize();
};
