// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../uber/uber_utils.js">

cr.define('help', function() {
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
      var title = loadTimeData.getString('helpTitle');
      uber.invokeMethodOnParent('setTitle', {title: title});

      $('product-license').innerHTML = loadTimeData.getString('productLicense');
      if (cr.isChromeOS) {
        $('product-os-license').innerHTML =
            loadTimeData.getString('productOsLicense');
      }

      var productTOS = $('product-tos');
      if (productTOS)
        productTOS.innerHTML = loadTimeData.getString('productTOS');

      $('get-help').onclick = function() {
        chrome.send('openHelpPage');
      };
      $('report-issue').onclick = function() {
        chrome.send('openFeedbackDialog');
      };

      this.maybeSetOnClick_($('more-info-expander'),
          this.toggleMoreInfo_.bind(this));

      this.maybeSetOnClick_($('promote'), function() {
        chrome.send('promoteUpdater');
      });
      this.maybeSetOnClick_($('relaunch'), function() {
        chrome.send('relaunchNow');
      });

      var channelChanger = $('channel-changer');
      if (channelChanger) {
        this.channelName_ = {
            'stable-channel': loadTimeData.getString('stable'),
            'beta-channel': loadTimeData.getString('beta'),
            'dev-channel': loadTimeData.getString('dev')
        };
        var self = this;
        channelChanger.onchange = function(event) {
          self.setReleaseChannel_(event.target.value);
        }
      }

      // Attempt to update.
      chrome.send('onPageLoaded');
    },

    /**
     * Toggles the visible state of the 'More Info' section.
     * @private
     */
    toggleMoreInfo_: function() {
      var moreInfo = $('more-info-container');
      var visible = moreInfo.className == 'visible';
      moreInfo.className = visible ? '' : 'visible';
      moreInfo.style.height = visible ? '' : moreInfo.scrollHeight + 'px';
      moreInfo.addEventListener('webkitTransitionEnd', function(event) {
        $('more-info-expander').textContent = visible ?
            loadTimeData.getString('showMoreInfo') :
            loadTimeData.getString('hideMoreInfo');
      });
    },

    /**
     * Assigns |method| to the onclick property of |el| if |el| exists.
     * @private
     */
    maybeSetOnClick_: function(el, method) {
      if (el)
        el.onclick = method;
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
            loadTimeData.getString('updateCheckStarted');
      } else if (status == 'updating') {
        this.setUpdateImage_('working');
        $('update-status').innerHTML = loadTimeData.getString('updating');
      } else if (status == 'nearly_updated') {
        this.setUpdateImage_('up-to-date');
        $('update-status').innerHTML =
            loadTimeData.getString('updateAlmostDone');
      } else if (status == 'updated') {
        this.setUpdateImage_('up-to-date');
        $('update-status').innerHTML = loadTimeData.getString('upToDate');
      } else if (status == 'failed') {
        this.setUpdateImage_('failed');
        $('update-status').innerHTML = message;
      }

      var container = $('update-status-container');
      if (container) {
        container.hidden = status == 'disabled';
        $('relaunch').hidden = status != 'nearly_updated';

        if (!cr.isMac)
          $('update-percentage').hidden = status != 'updating';
      }
    },

    /**
     * @private
     */
    setProgress_: function(progress) {
      $('update-percentage').innerHTML = progress + '%';
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
      $('channel-changer-container').hidden = !enabled;
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
      $('channel-change-confirmation').hidden = false;
      $('channel-change-confirmation').textContent = loadTimeData.getStringF(
          'channel-changed', this.channelName_[channel]);
    },

    /**
     * Sets the value of the "Build Date" field of the "More Info" section.
     * @param {String} buildDate The date of the build.
     * @private
     */
    setBuildDate_: function(buildDate) {
      $('build-date-container').classList.remove('empty');
      $('build-date').textContent = buildDate;
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

  HelpPage.setObsoleteOS = function(obsolete) {
    HelpPage.getInstance().setObsoleteOS_(obsolete);
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

  HelpPage.setBuildDate = function(buildDate) {
    HelpPage.getInstance().setBuildDate_(buildDate);
  }

  // Export
  return {
    HelpPage: HelpPage
  };
});

/**
 * onload listener to initialize the HelpPage.
 */
window.onload = function() {
  help.HelpPage.getInstance().initialize();
};
