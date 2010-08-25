// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /**
   * AlertOverlay class
   * Encapsulated handling of a generic alert.
   * @class
   */
  function AlertOverlay() {
    OptionsPage.call(this, 'alertOverlay', '', 'alertOverlay');
  }

  cr.addSingletonGetter(AlertOverlay);

  AlertOverlay.prototype = {
    // Inherit AlertOverlay from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);
    }
  };

  /**
   * Show an alert overlay with the given message, button titles, and
   * callbacks.
   * @param {string} message The message to dispaly to the user.
   * @param {string} okTitle The title of the OK button.  Can be undefined.
   * @param {string} cancelTitle The title of the cancel button.  Can be
   *     undefined.
   * @param {function} okCallback A function to be called when the user presses
   *     the ok button.  The alert window will be closed automatically.  Can be
   *     undefined.
   * @param {function} cancelCallback A function to be called when the user
   *     presses the cancel button.  The alert window will be closed
   *     automatically.  Can be undefined.
   */
  AlertOverlay.show = function(message, okTitle, cancelTitle, okCallback,
                               cancelCallback) {
    $('alertOverlayMessage').textContent = message;
    $('alertOverlayOk').textContent =
        (okTitle != undefined ? okTitle
                              : LocalStrings.getString('ok'));
    $('alertOverlayCancel').textContent =
        (cancelTitle != undefined ? cancelTitle
                                  : LocalStrings.getString('cancel'));

    AlertOverlay.getInstance().okCallback = okCallback;
    AlertOverlay.getInstance().cancelCallback = cancelCallback;

    OptionsPage.showOverlay('alertOverlay');
  }

  /**
   * Handle the 'ok' button.  Clear the overlay and call the ok callback if
   * available.
   */
  AlertOverlay.handleOk = function() {
    OptionsPage.clearOverlays();
    if (AlertOverlay.getInstance().okCallback != undefined) {
      AlertOverlay.getInstance().okCallback.call();
    }
  }

  /**
   * Handle the 'cancel' button.  Clear the overlay and call the cancel callback
   * if available.
   */
  AlertOverlay.handleCancel = function() {
    OptionsPage.clearOverlays();
    if (AlertOverlay.getInstance().cancelCallback != undefined) {
      AlertOverlay.getInstance().cancelCallback.call();
    }
  }

  // Export
  return {
    AlertOverlay: AlertOverlay
  };

});
