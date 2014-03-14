// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var ControlledSettingIndicator =
                    options.ControlledSettingIndicator;

  /**
   * A variant of the {@link ControlledSettingIndicator} that shows the status
   * of the hotword search setting, including a bubble to show setup errors
   * (such as failures to download extension resources).
   * @constructor
   * @extends {HTMLSpanElement}
   */
  var HotwordSearchSettingIndicator = cr.ui.define('span');

  HotwordSearchSettingIndicator.prototype = {
    __proto__: ControlledSettingIndicator.prototype,

    /**
     * Decorates the base element to show the proper icon.
     * @override
     */
    decorate: function() {
      ControlledSettingIndicator.prototype.decorate.call(this);
      this.hidden = true;
    },

    /* Handle changes to the associated pref by hiding any currently visible
     * bubble.
     * @param {Event} event Pref change event.
     * @override
     */
    handlePrefChange: function(event) {
      OptionsPage.hideBubble();
    },

    /**
     * Sets the variable tracking thesection which becomes disabled if an
     * error exists.
     * @param {HTMLElement} section The section to disable.
     */
    set disabledOnErrorSection(section) {
      this.disabledOnErrorSection_ = section;
    },

    /**
     * Assigns a value to the error message and updates the hidden state
     * and whether the disabled section is disabled or not.
     * @param {string} errorMsg The error message to be displayed. If none,
     *                          there is no error.
     */
    set errorText(errorMsg) {
      this.setAttribute('controlled-by', 'policy');
      this.errorText_ = errorMsg;
      if (errorMsg)
        this.hidden = false;
      if (this.disabledOnErrorSection_)
        this.disabledOnErrorSection_.disabled = (errorMsg ? true : false);
    },

    /**
     * Toggles showing and hiding the error message bubble. An empty
     * |errorText_| indicates that there is no error message. So the bubble
     * only be shown if |errorText_| has a value.
     */
    toggleBubble_: function() {
      if (this.showingBubble) {
        OptionsPage.hideBubble();
        return;
      }

      if (!this.errorText_)
        return;

      var self = this;
      // Create the DOM tree for the bubble content.
      var action = document.createElement('button');
      action.classList.add('default-button');
      action.classList.add('controlled-setting-bubble-action');
      action.textContent = loadTimeData.getString('hotwordRetryDownloadButton');
      action.addEventListener('click', function(event) {
        self.retryDownload_();
      });

      var buttonStrip = document.createElement('div');
      buttonStrip.classList.add('button-strip');
      buttonStrip.reversed = true;
      buttonStrip.appendChild(action);

      var actionContainer = document.createElement('div');
      actionContainer.classList.add('action-area');
      actionContainer.appendChild(buttonStrip);

      var closeButton = document.createElement('div');
      closeButton.classList.add('close-button');

      var text = document.createElement('p');
      text.textContent = this.errorText_;

      var container = document.createElement('div');
      container.appendChild(closeButton);
      container.appendChild(text);
      container.appendChild(actionContainer);

      var content = document.createElement('div');
      content.appendChild(container);

      OptionsPage.showBubble(content, this.image, this, this.location);
    },

    retryDownload_: function() {
      chrome.send('requestHotwordSetupRetry');
      OptionsPage.hideBubble();
    },
  };

  // Export
  return {
    HotwordSearchSettingIndicator: HotwordSearchSettingIndicator
  };
});
