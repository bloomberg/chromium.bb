// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('help', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * Encapsulated handling of the About page. Called 'help' internally to avoid
   * confusion with generic AboutUI (about:memory, about:sandbox, etc.).
   */
  function HelpPage() {
    var id = loadTimeData.valueExists('aboutOverlayTabTitle') ?
      'aboutOverlayTabTitle' : 'aboutTitle';
    Page.call(this, 'help', loadTimeData.getString(id), 'help-page');
  }

  cr.addSingletonGetter(HelpPage);

  HelpPage.prototype = {
    __proto__: Page.prototype,

    /**
     * True if after update powerwash button should be displayed.
     * @private
     */
    powerwashAfterUpdate_: false,

    /**
     * List of the channel names.
     * @private
     */
    channelList_: ['dev-channel', 'beta-channel', 'stable-channel'],

    /**
     * Name of the channel the device is currently on.
     * @private
     */
    currentChannel_: null,

    /**
     * Name of the channel the device is supposed to be on.
     * @private
     */
    targetChannel_: null,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

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
<if expr="_google_chrome">
      $('report-issue').onclick = function() {
        chrome.send('openFeedbackDialog');
      };
</if>

      this.maybeSetOnClick_($('more-info-expander'),
          this.toggleMoreInfo_.bind(this));

      this.maybeSetOnClick_($('promote'), function() {
        chrome.send('promoteUpdater');
      });
      this.maybeSetOnClick_($('relaunch'), function() {
        chrome.send('relaunchNow');
      });
      if (cr.isChromeOS) {
        this.maybeSetOnClick_($('relaunch-and-powerwash'), function() {
          chrome.send('relaunchAndPowerwash');
        });

        this.channelTable_ = {
          'stable-channel': {
            'name': loadTimeData.getString('stable'),
            'label': loadTimeData.getString('currentChannelStable'),
          },
          'beta-channel': {
            'name': loadTimeData.getString('beta'),
            'label': loadTimeData.getString('currentChannelBeta')
          },
          'dev-channel': {
            'name': loadTimeData.getString('dev'),
            'label': loadTimeData.getString('currentChannelDev')
          }
        };
      }

      var self = this;
      var channelChanger = $('channel-changer');
      if (channelChanger) {
        channelChanger.onchange = function(event) {
          self.setChannel_(event.target.value, false);
        };
      }

      if (cr.isChromeOS) {
        // Add event listener for the close button when shown as an overlay.
        if ($('about-done')) {
          $('about-done').addEventListener('click', function() {
            PageManager.closeOverlay();
          });
        }

        $('change-channel').onclick = function() {
          PageManager.showPageByName('channel-change-page', false);
        };

        var channelChangeDisallowedError = document.createElement('div');
        channelChangeDisallowedError.className = 'channel-change-error-bubble';

        var channelChangeDisallowedIcon = document.createElement('div');
        channelChangeDisallowedIcon.classList.add('help-page-icon-large');
        channelChangeDisallowedIcon.classList.add('channel-change-error-icon');
        channelChangeDisallowedError.appendChild(channelChangeDisallowedIcon);

        var channelChangeDisallowedText = document.createElement('div');
        channelChangeDisallowedText.className = 'channel-change-error-text';
        channelChangeDisallowedText.textContent =
            loadTimeData.getString('channelChangeDisallowedMessage');
        channelChangeDisallowedError.appendChild(channelChangeDisallowedText);

        $('channel-change-disallowed-icon').onclick = function() {
          PageManager.showBubble(channelChangeDisallowedError,
                                 $('channel-change-disallowed-icon'),
                                 $('help-container'),
                                 cr.ui.ArrowLocation.TOP_END);
        };
      }

      // Attempt to update.
      chrome.send('onPageLoaded');
    },

    /** @override */
    didClosePage: function() {
      this.setMoreInfoVisible_(false);
    },

    /**
     * Sets the visible state of the 'More Info' section.
     * @param {boolean} visible Whether the section should be visible.
     * @private
     */
    setMoreInfoVisible_: function(visible) {
      var moreInfo = $('more-info-container');
      if (visible == moreInfo.classList.contains('visible'))
        return;

      moreInfo.classList.toggle('visible', visible);
      moreInfo.style.height = visible ? moreInfo.scrollHeight + 'px' : '';
      moreInfo.addEventListener('webkitTransitionEnd', function(event) {
        $('more-info-expander').textContent = visible ?
            loadTimeData.getString('hideMoreInfo') :
            loadTimeData.getString('showMoreInfo');
      });
    },

    /**
     * Toggles the visible state of the 'More Info' section.
     * @private
     */
    toggleMoreInfo_: function() {
      var moreInfo = $('more-info-container');
      this.setMoreInfoVisible_(!moreInfo.classList.contains('visible'));
    },

    /**
     * Assigns |method| to the onclick property of |el| if |el| exists.
     * @param {HTMLElement} el The element on which to set the click handler.
     * @param {function} method The click handler.
     * @private
     */
    maybeSetOnClick_: function(el, method) {
      if (el)
        el.onclick = method;
    },

    /**
     * @param {string} state The state of the update.
     * private
     */
    setUpdateImage_: function(state) {
      $('update-status-icon').className = 'help-page-icon ' + state;
    },

    /**
     * @return {boolean} True, if new channel switcher UI is used,
     *    false otherwise.
     * @private
     */
    isNewChannelSwitcherUI_: function() {
      return !loadTimeData.valueExists('disableNewChannelSwitcherUI');
    },

    /**
     * @return {boolean} True if target and current channels are not null and
     *     not equal.
     * @private
     */
    channelsDiffer_: function() {
      var current = this.currentChannel_;
      var target = this.targetChannel_;
      return (current != null && target != null && current != target);
    },

    /**
     * @param {string} status The status of the update.
     * @param {string} message Failure message to display.
     * @private
     */
    setUpdateStatus_: function(status, message) {
      if (cr.isMac &&
          $('update-status-message') &&
          $('update-status-message').hidden) {
        // Chrome has reached the end of the line on this system. The
        // update-obsolete-system message is displayed. No other auto-update
        // status should be displayed.
        return;
      }

      var channel = this.targetChannel_;
      if (status == 'checking') {
        this.setUpdateImage_('working');
        $('update-status-message').innerHTML =
            loadTimeData.getString('updateCheckStarted');
      } else if (status == 'updating') {
        this.setUpdateImage_('working');
        if (this.channelsDiffer_()) {
          $('update-status-message').innerHTML =
              loadTimeData.getStringF('updatingChannelSwitch',
                                      this.channelTable_[channel].label);
        } else {
          $('update-status-message').innerHTML =
              loadTimeData.getStringF('updating');
        }
      } else if (status == 'nearly_updated') {
        this.setUpdateImage_('up-to-date');
        if (this.channelsDiffer_()) {
          $('update-status-message').innerHTML =
              loadTimeData.getString('successfulChannelSwitch');
        } else {
          $('update-status-message').innerHTML =
              loadTimeData.getString('updateAlmostDone');
        }
      } else if (status == 'updated') {
        this.setUpdateImage_('up-to-date');
        $('update-status-message').innerHTML =
            loadTimeData.getString('upToDate');
      } else if (status == 'failed') {
        this.setUpdateImage_('failed');
        $('update-status-message').innerHTML = message;
      }

      // Following invariant must be established at the end of this function:
      // { ~$('relaunch_and_powerwash').hidden -> $('relaunch').hidden }
      var relaunchAndPowerwashHidden = true;
      if ($('relaunch-and-powerwash')) {
        // It's allowed to do powerwash only for customer devices,
        // when user explicitly decides to update to a more stable
        // channel.
        relaunchAndPowerwashHidden =
            !this.powerwashAfterUpdate_ || status != 'nearly_updated';
        $('relaunch-and-powerwash').hidden = relaunchAndPowerwashHidden;
      }

      var container = $('update-status-container');
      if (container) {
        container.hidden = status == 'disabled';
        $('relaunch').hidden =
            (status != 'nearly_updated') || !relaunchAndPowerwashHidden;

        if (!cr.isMac)
          $('update-percentage').hidden = status != 'updating';
      }
    },

    /**
     * @param {number} progress The percent completion.
     * @private
     */
    setProgress_: function(progress) {
      $('update-percentage').innerHTML = progress + '%';
    },

    /**
     * @param {string} message The allowed connection types message.
     * @private
     */
    setAllowedConnectionTypesMsg_: function(message) {
      $('allowed-connection-types-message').innerText = message;
    },

    /**
     * @param {boolean} visible Whether to show the message.
     * @private
     */
    showAllowedConnectionTypesMsg_: function(visible) {
      $('allowed-connection-types-message').hidden = !visible;
    },

    /**
     * @param {string} state The promote state to set.
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
     * @param {boolean} obsolete Whether the system is obsolete.
     * @private
     */
    setObsoleteSystem_: function(obsolete) {
      if (cr.isMac && $('update-obsolete-system-container')) {
        $('update-obsolete-system-container').hidden = !obsolete;
      }
    },

    /**
     * @param {boolean} endOfTheLine Whether the train has rolled into
     *     the station.
     * @private
     */
    setObsoleteSystemEndOfTheLine_: function(endOfTheLine) {
      if (cr.isMac &&
          $('update-obsolete-system-container') &&
          !$('update-obsolete-system-container').hidden &&
          $('update-status-message')) {
        $('update-status-message').hidden = endOfTheLine;
        if (endOfTheLine) {
          this.setUpdateImage_('failed');
        }
      }
    },

    /**
     * @param {string} version Version of Chrome OS.
     * @private
     */
    setOSVersion_: function(version) {
      if (!cr.isChromeOS)
        console.error('OS version unsupported on non-CrOS');

      $('os-version').parentNode.hidden = (version == '');
      $('os-version').textContent = version;
    },

    /**
     * @param {string} firmware Firmware on Chrome OS.
     * @private
     */
    setOSFirmware_: function(firmware) {
      if (!cr.isChromeOS)
        console.error('OS firmware unsupported on non-CrOS');

      $('firmware').parentNode.hidden = (firmware == '');
      $('firmware').textContent = firmware;
    },

    /**
     * Updates name of the current channel, i.e. the name of the
     * channel the device is currently on.
     * @param {string} channel The name of the current channel.
     * @private
     */
    updateCurrentChannel_: function(channel) {
      if (this.channelList_.indexOf(channel) < 0)
        return;
      $('current-channel').textContent = loadTimeData.getStringF(
          'currentChannel', this.channelTable_[channel].label);
      this.currentChannel_ = channel;
      help.ChannelChangePage.updateCurrentChannel(channel);
    },

    /**
     * @param {boolean} enabled True if the release channel can be enabled.
     * @private
     */
    updateEnableReleaseChannel_: function(enabled) {
      this.updateChannelChangerContainerVisibility_(enabled);
      $('change-channel').disabled = !enabled;
      $('channel-change-disallowed-icon').hidden = enabled;
    },

    /**
     * Sets the device target channel.
     * @param {string} channel The name of the target channel.
     * @param {boolean} isPowerwashAllowed True iff powerwash is allowed.
     * @private
     */
    setChannel_: function(channel, isPowerwashAllowed) {
      this.powerwashAfterUpdate_ = isPowerwashAllowed;
      this.targetChannel_ = channel;
      chrome.send('setChannel', [channel, isPowerwashAllowed]);
      $('channel-change-confirmation').hidden = false;
      $('channel-change-confirmation').textContent = loadTimeData.getStringF(
          'channel-changed', this.channelTable_[channel].name);
    },

    /**
     * Sets the value of the "Build Date" field of the "More Info" section.
     * @param {string} buildDate The date of the build.
     * @private
     */
    setBuildDate_: function(buildDate) {
      $('build-date-container').classList.remove('empty');
      $('build-date').textContent = buildDate;
    },

    /**
     * Updates channel-change-page-container visibility according to
     * internal state.
     * @private
     */
    updateChannelChangePageContainerVisibility_: function() {
      if (!this.isNewChannelSwitcherUI_()) {
        $('channel-change-page-container').hidden = true;
        return;
      }
      $('channel-change-page-container').hidden =
          !help.ChannelChangePage.isPageReady();
    },

    /**
     * Updates channel-changer dropdown visibility if |visible| is
     * true and new channel switcher UI is disallowed.
     * @param {boolean} visible True if channel-changer should be
     *     displayed, false otherwise.
     * @private
     */
    updateChannelChangerContainerVisibility_: function(visible) {
      if (this.isNewChannelSwitcherUI_()) {
        $('channel-changer').hidden = true;
        return;
      }
      $('channel-changer').hidden = !visible;
    },
  };

  HelpPage.setUpdateStatus = function(status, message) {
    HelpPage.getInstance().setUpdateStatus_(status, message);
  };

  HelpPage.setProgress = function(progress) {
    HelpPage.getInstance().setProgress_(progress);
  };

  HelpPage.setAndShowAllowedConnectionTypesMsg = function(message) {
    HelpPage.getInstance().setAllowedConnectionTypesMsg_(message);
    HelpPage.getInstance().showAllowedConnectionTypesMsg_(true);
  };

  HelpPage.showAllowedConnectionTypesMsg = function(visible) {
    HelpPage.getInstance().showAllowedConnectionTypesMsg_(visible);
  };

  HelpPage.setPromotionState = function(state) {
    HelpPage.getInstance().setPromotionState_(state);
  };

  HelpPage.setObsoleteSystem = function(obsolete) {
    HelpPage.getInstance().setObsoleteSystem_(obsolete);
  };

  HelpPage.setObsoleteSystemEndOfTheLine = function(endOfTheLine) {
    HelpPage.getInstance().setObsoleteSystemEndOfTheLine_(endOfTheLine);
  };

  HelpPage.setOSVersion = function(version) {
    HelpPage.getInstance().setOSVersion_(version);
  };

  HelpPage.setOSFirmware = function(firmware) {
    HelpPage.getInstance().setOSFirmware_(firmware);
  };

  HelpPage.updateIsEnterpriseManaged = function(isEnterpriseManaged) {
    if (!cr.isChromeOS)
      return;
    help.ChannelChangePage.updateIsEnterpriseManaged(isEnterpriseManaged);
  };

  HelpPage.updateCurrentChannel = function(channel) {
    if (!cr.isChromeOS)
      return;
    HelpPage.getInstance().updateCurrentChannel_(channel);
  };

  HelpPage.updateTargetChannel = function(channel) {
    if (!cr.isChromeOS)
      return;
    help.ChannelChangePage.updateTargetChannel(channel);
  };

  HelpPage.updateEnableReleaseChannel = function(enabled) {
    HelpPage.getInstance().updateEnableReleaseChannel_(enabled);
  };

  HelpPage.setChannel = function(channel, isPowerwashAllowed) {
    HelpPage.getInstance().setChannel_(channel, isPowerwashAllowed);
  };

  HelpPage.setBuildDate = function(buildDate) {
    HelpPage.getInstance().setBuildDate_(buildDate);
  };

  HelpPage.updateChannelChangePageContainerVisibility = function() {
    HelpPage.getInstance().updateChannelChangePageContainerVisibility_();
  };

  // Export
  return {
    HelpPage: HelpPage
  };
});
