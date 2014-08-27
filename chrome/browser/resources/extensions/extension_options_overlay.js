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
     * @type {?function(HTMLDivElement)} Function that receives the element to
     *     show in the overlay.
     * @private
     */
    showOverlay_: null,

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
     * @param {Event} event The click event.
     * @private
     */
    handleDismiss_: function(event) {
      this.setVisible_(false);
      var extensionoptions =
          $('extension-options-overlay-guest')
              .querySelector('extensionoptions');

      if (extensionoptions)
        $('extension-options-overlay-guest').removeChild(extensionoptions);

      $('extension-options-overlay-icon').removeAttribute('src');

      // Remove the options query string.
      uber.replaceState({}, '');
    },

    /**
     * Associate an extension with the overlay and display it.
     * @param {string} extensionId The id of the extension whose options page
     *     should be displayed in the overlay.
     * @param {string} extensionName The name of the extension, which is used
     *     as the header of the overlay.
     * @param {string} extensionIcon The URL of the extension's icon.
     */
    setExtensionAndShowOverlay: function(extensionId,
                                         extensionName,
                                         extensionIcon) {
      $('extension-options-overlay-title').textContent = extensionName;
      $('extension-options-overlay-icon').src = extensionIcon;

      this.setVisible_(true);

      var extensionoptions = new ExtensionOptions();
      extensionoptions.extension = extensionId;
      extensionoptions.autosize = 'on';

      // The <extensionoptions> content's size needs to be restricted to the
      // bounds of the overlay window. The overlay gives a min width and
      // max height, but the maxheight does not include our header height
      // (title and close button), so we need to subtract that to get the
      // max height for the extension options.
      var headerHeight = $('extension-options-overlay-header').offsetHeight;
      var overlayMaxHeight =
          parseInt($('extension-options-overlay').style.maxHeight);
      extensionoptions.maxheight = overlayMaxHeight - headerHeight;

      extensionoptions.minwidth =
          parseInt(window.getComputedStyle($('extension-options-overlay'))
              .minWidth);

      extensionoptions.setDeferAutoSize(true);

      extensionoptions.onclose = function() {
        cr.dispatchSimpleEvent($('overlay'), 'cancelOverlay');
      }.bind(this);

      // Resize the overlay if the <extensionoptions> changes size.
      extensionoptions.onsizechanged = function(evt) {
        var overlayStyle =
            window.getComputedStyle($('extension-options-overlay'));
        var oldWidth = parseInt(overlayStyle.width);
        var oldHeight = parseInt(overlayStyle.height);

        // animationTime is the amount of time in ms that will be used to resize
        // the overlay. It is calculated by multiplying the pythagorean distance
        // between old and the new size (in px) with a constant speed of
        // 0.25 ms/px.
        var animationTime = 0.25 * Math.sqrt(
            Math.pow(evt.newWidth - oldWidth, 2) +
            Math.pow(evt.newHeight - oldHeight, 2));

        var player = $('extension-options-overlay').animate([
          {width: oldWidth + 'px', height: oldHeight + 'px'},
          {width: evt.newWidth + 'px', height: evt.newHeight + 'px'}
        ], {
          duration: animationTime,
          delay: 0
        });

        player.onfinish = function(e) {
          // Allow the <extensionoptions> to autosize now that the overlay
          // has resized, and move it back on-screen.
          extensionoptions.resumeDeferredAutoSize();
          $('extension-options-overlay-guest').style.position = 'static';
          $('extension-options-overlay-guest').style.left = 'auto';
        };
      }.bind(this);

      // Don't allow the <extensionoptions> to autosize until the overlay
      // animation is complete.
      extensionoptions.setDeferAutoSize(true);

      // Move the <extensionoptions> off screen until the overlay is ready
      $('extension-options-overlay-guest').style.position = 'fixed';
      $('extension-options-overlay-guest').style.left =
          window.outerWidth + 'px';

      $('extension-options-overlay-guest').appendChild(extensionoptions);
    },

    /**
     * Toggles the visibility of the ExtensionOptionsOverlay.
     * @param {boolean} isVisible Whether the overlay should be visible.
     * @private
     */
    setVisible_: function(isVisible) {
      this.showOverlay_(isVisible ?
          /** @type {HTMLDivElement} */($('extension-options-overlay')) :
          null);
    }
  };

  // Export
  return {
    ExtensionOptionsOverlay: ExtensionOptionsOverlay
  };
});
