// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * The ExtensionOptionsOverlay will show an extension's options page using
   * an <extensionoptions> element.
   * @constructor
   */
  function ExtensionOptionsOverlay() {}

  cr.addSingletonGetter(ExtensionOptionsOverlay);

  ExtensionOptionsOverlay.prototype = {
    /**
     * The function that shows the given element in the overlay.
     * @type {Function}
     * @param {HTMLElement} The element to show in the overlay.
     * @private
     */
    showOverlay_: undefined,

    /**
     * Initialize the page.
     * @param {function(HTMLDivElement)} showOverlay The function to show or
     *     hide the ExtensionOptionsOverlay; this should take a single parameter
     *     which is either the overlay Div if the overlay should be displayed,
     *     or null if the overlay should be hidden.
     */
    initializePage: function(showOverlay) {
      var overlay = $('overlay');

      cr.ui.overlay.setupOverlay(overlay);
      cr.ui.overlay.globalInitialization();
      overlay.addEventListener('cancelOverlay', this.handleDismiss_.bind(this));

      this.showOverlay_ = showOverlay;
    },

    /**
     * Handles a click on the close button.
     * @param {Event} e The click event.
     * @private
     */
    handleDismiss_: function(event) {
      this.setVisible_(false);
      var extensionoptions =
          $('extension-options-overlay-guest')
              .querySelector('extensionoptions');

      if (extensionoptions)
        $('extension-options-overlay-guest').removeChild(extensionoptions);
    },

    /**
     * Associate an extension with the overlay and display it.
     * @param {string} extensionId The id of the extension whose options page
     *     should be displayed in the overlay.
     * @param {string} extensionName The name of the extension, which is used
     *     as the header of the overlay.
     */
    setExtensionAndShowOverlay: function(extensionId, extensionName) {
      var extensionoptions = new ExtensionOptions();
      extensionoptions.extension = extensionId;
      extensionoptions.autosize = 'on';

      extensionoptions.onclose = function() {
        this.handleDismiss_();
      }.bind(this);

      // TODO(ericzeng): Resize in a non-jarring way.
      extensionoptions.onsizechanged = function(evt) {
        $('extension-options-overlay').style.width = evt.width;
        $('extension-options-overlay').style.height = evt.height;
      }.bind(this);

      $('extension-options-overlay-guest').appendChild(extensionoptions);

      $('extension-options-overlay-title').textContent = extensionName;

      this.setVisible_(true);
    },

    /**
     * Toggles the visibility of the ExtensionOptionsOverlay.
     * @param {boolean} isVisible Whether the overlay should be visible.
     * @private
     */
    setVisible_: function(isVisible) {
      this.showOverlay_(isVisible ? $('extension-options-overlay') : null);
    }
  };

  // Export
  return {
    ExtensionOptionsOverlay: ExtensionOptionsOverlay
  };
});
