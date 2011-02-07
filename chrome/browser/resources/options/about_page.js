// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
    OptionsPage.call(this, 'about', templateData.aboutPage, 'aboutPage');
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
        $('channelSelect').onchange = function(event) {
          var channel = event.target.value;
          chrome.send('SetReleaseTrack', [channel]);
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

    // Updates the selected option in 'channelSelect' <select> element.
    updateSelectedOption_: function(value) {
      var options = $('channelSelect').querySelectorAll('option');
      for (var i = 0; i < options.length; i++) {
        var option = options[i];
        if (option.value == value) {
          option.selected = true;
        }
      }
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
