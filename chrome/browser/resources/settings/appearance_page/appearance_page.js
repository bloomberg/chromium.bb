// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * A variation of chrome.send which allows the client to receive a direct
 * callback.
 * @param {string} methodName The name of the WebUI handler API.
 * @param {Function} callback A callback function which is called (indirectly)
 *     by the WebUI handler.
 */
var ChromeSendWithCallback = function(methodName, callback) {
  var self = ChromeSendWithCallback;
  if (typeof self.functionCallbackMap_ == 'undefined') {
    self.functionCallbackMap_ = new Map();
    self.functionCallbackCounter_ = 0;

    window.chromeSendWithCallback = function(id) {
      self.functionCallbackMap_[id].apply(
          null, Array.prototype.slice.call(arguments, 1));
      self.functionCallbackMap_.delete(id);
    };
  }

  var id = methodName + self.functionCallbackCounter_++;
  self.functionCallbackMap_[id] = callback;
  chrome.send(methodName, ['chromeSendWithCallback', id]);
};


/**
 * Registers an event listener for an event fired from WebUI handlers (i.e.,
 * fired from the browser).
 * @param {string} event The event to listen to.
 * @param {Function} callback The callback run when the event is fired.
 */
var ChromeSendAddEventListener = function(event, callback) {
  var self = ChromeSendAddEventListener;
  if (typeof self.eventListenerMap_ == 'undefined') {
    self.eventListenerMap_ = new Map();

    window.chromeSendListenerCallback = function(event) {
      var listenerCallbacks = self.eventListenerMap_[event];
      for (var i = 0; i < listenerCallbacks.length; i++) {
        listenerCallbacks[i].apply(
          null, Array.prototype.slice.call(arguments, 1));
      }
    };
  }

  if (!self.eventListenerMap_.has(event)) {
    var listenerCallbacks = [callback];
    self.eventListenerMap_[event] = listenerCallbacks;
  }
};



/**
 * 'cr-settings-appearance-page' is the settings page containing appearance
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <cr-settings-appearance-page prefs="{{prefs}}">
 *      </cr-settings-appearance-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-appearance-page
 */
Polymer({
  is: 'cr-settings-appearance-page',

  /** @override */
  attached: function() {
    // Query the initial state.
    ChromeSendWithCallback('getResetThemeEnabled',
      this.setResetThemeEnabled.bind(this));

    // Set up the change event listener.
    ChromeSendAddEventListener(
      'reset-theme-enabled-changed',
      this.setResetThemeEnabled.bind(this));
  },

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Route for the page.
     */
    route: String,

    /**
     * Whether the page is a subpage.
     */
    subpage: {
      type: Boolean,
      value: false,
      readOnly: true,
    },

    /**
     * ID of the page.
     */
    PAGE_ID: {
      type: String,
      value: 'appearance',
      readOnly: true,
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() {
        return loadTimeData.getString('appearancePageTitle');
      },
    },

    /**
     * Name of the 'iron-icon' to show.
     */
    icon: {
      type: String,
      value: 'home',
      readOnly: true,
    },
  },

  setResetThemeEnabled: function(enabled) {
    this.$.resetTheme.disabled = !enabled;
  },

  /** @private */
  openThemesGallery_: function() {
    window.open(loadTimeData.getString('themesGalleryUrl'));
  },

  /** @private */
  resetTheme_: function() {
    chrome.send('resetTheme');
  },
});
