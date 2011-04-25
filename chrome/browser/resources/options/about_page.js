// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

  /**
   * The number of milliseconds used for showing a message.
   * @type {number}
   */
  const MESSAGE_DELAY_MS = 1000;  // 1 sec.

  /**
   * Encapsulated handling of about page.
   */
  function AboutPage() {
    OptionsPage.call(this, 'about', templateData.aboutPageTabTitle,
                     'aboutPage');
  }

  cr.addSingletonGetter(AboutPage);

  AboutPage.prototype = {
    // Inherit AboutPage from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * The queue is used for updating the status message with delay, like:
     * [["Check for update...", 1000], ["Chrome OS is up to date", 0]]
     * @type {!Array.<!Array>}
     */
    statusMessageQueue_: [],

    /**
     * True if the status message queue flush started.
     * @type {boolean}
     */
    statusMessageQueueFlushStarted_: false,

    /**
     * The selected release channel.
     * @type {string}
     */
    selectedChannel_: '',

    // Initialize AboutPage.
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      $('checkNow').onclick = function(event) {
        chrome.send('CheckNow');
      };

      $('moreInfoButton').onclick = function(event) {
        $('aboutPageLessInfo').classList.add('hidden');
        $('aboutPageMoreInfo').classList.remove('hidden');
      };

      if (!AccountsOptions.currentUserIsOwner()) {
        $('channelSelect').disabled = true;
      } else {
        var self = this;
        $('channelSelect').onchange = function(event) {
          self.selectedOptionOnChange_(event.target.value);
        };
      }

      // Notify the handler that the page is ready.
      chrome.send('PageReady');
    },

    // Update the Default Browsers section based on the current state.
    updateOSVersion_: function(versionString) {
      $('osVersion0').textContent = versionString;
      $('osVersion1').textContent = versionString;
    },

    updateOSFirmware_: function(firmwareString) {
      $('osFirmware0').textContent = firmwareString;
      $('osFirmware1').textContent = firmwareString;
    },

    /**
     * Updates the status message like "Checking for update...".
     * @param {string} message The message to be shown.
     * @param {boolean} insertDelay show the message for a while.
     * @private
     */
    updateStatus_: function(message, insertDelay) {
      // Add the message to the queue with delay if needed.
      // The delay is inserted so users can read the message.
      var delayMs = insertDelay ? MESSAGE_DELAY_MS : 0;
      this.statusMessageQueue_.push([message, delayMs]);
      // Start the periodic flusher if not started.
      if (this.statusMessageQueueFlushStarted_ == false) {
        this.flushStatusMessageQueuePeriodically_();
      }
    },

    /**
     * Flushes the status message queue periodically using a timer.
     * @private
     */
    flushStatusMessageQueuePeriodically_: function() {
      // Stop the periodic flusher if the queue becomes empty.
      if (this.statusMessageQueue_.length == 0) {
        this.statusMessageQueueFlushStarted_ = false;
        return;
      }
      this.statusMessageQueueFlushStarted_ = true;

      // Update the status message.
      var pair = this.statusMessageQueue_.shift();
      var message = pair[0];
      var delayMs = pair[1];
      $('updateStatus').textContent = message;

      // Schedule the next flush with delay as needed.
      var self = this;
      window.setTimeout(
          function() { self.flushStatusMessageQueuePeriodically_() },
          delayMs);
    },

    updateEnable_: function(enable) {
      $('checkNow').disabled = !enable;
    },

    selectedOptionOnChange_: function(value) {
      if (value == 'dev-channel') {
        // Open confirm dialog.
        var self = this;
        AlertOverlay.show(
          localStrings.getString('channel_warning_header'),
          localStrings.getString('channel_warning_text'),
          localStrings.getString('ok'),
          localStrings.getString('cancel'),
          function() {
            // Ok, so set release track and update selected channel.
            $('channelWarningBlock').hidden = false;
            chrome.send('SetReleaseTrack', [value]);
            self.selectedChannel_ = value; },
          function() {
            // Cancel, so switch back to previous selected channel.
            self.updateSelectedOption_(self.selectedChannel_); }
          );
      } else {
        $('channelWarningBlock').hidden = true;
        chrome.send('SetReleaseTrack', [value]);
        this.selectedChannel_ = value;
      }
    },

    // Updates the selected option in 'channelSelect' <select> element.
    updateSelectedOption_: function(value) {
      var options = $('channelSelect').querySelectorAll('option');
      for (var i = 0; i < options.length; i++) {
        var option = options[i];
        if (option.value == value) {
          option.selected = true;
          this.selectedChannel_ = value;
        }
      }
      if (value == 'dev-channel')
        $('channelWarningBlock').hidden = false;
    },

    // Changes the "check now" button to "restart now" button.
    changeToRestartButton_: function() {
      $('checkNow').textContent = localStrings.getString('restart_now');
      $('checkNow').disabled = false;
      $('checkNow').onclick = function(event) {
        chrome.send('RestartNow');
      };
    },
  };

  AboutPage.updateOSVersionCallback = function(versionString) {
    AboutPage.getInstance().updateOSVersion_(versionString);
  };

  AboutPage.updateOSFirmwareCallback = function(firmwareString) {
    AboutPage.getInstance().updateOSFirmware_(firmwareString);
  };

  AboutPage.updateStatusCallback = function(message, insertDelay) {
    AboutPage.getInstance().updateStatus_(message, insertDelay);
  };

  AboutPage.updateEnableCallback = function(enable) {
    AboutPage.getInstance().updateEnable_(enable);
  };

  AboutPage.updateSelectedOptionCallback = function(value) {
    AboutPage.getInstance().updateSelectedOption_(value);
  };

  AboutPage.setUpdateImage = function(state) {
    $('updateIcon').className= 'update-icon ' + state;
  };

  AboutPage.changeToRestartButton = function() {
    AboutPage.getInstance().changeToRestartButton_();
  };

  // Export
  return {
    AboutPage: AboutPage
  };

});
