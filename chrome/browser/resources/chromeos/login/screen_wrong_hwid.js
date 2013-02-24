// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview wrong HWID screen implementation.
 */

cr.define('oobe', function() {
  /**
   * Creates a new screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var WrongHWIDScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  WrongHWIDScreen.register = function() {
    var screen = $('wrong-hwid');
    WrongHWIDScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  WrongHWIDScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      $('skip-hwid-warning-link').addEventListener('click', function(event) {
        chrome.send('wrongHWIDOnSkip');
      });
      this.updateLocalizedContent();
    },

    /**
     * Updates state of login header so that necessary buttons are displayed.
     **/
    onBeforeShow: function(data) {
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.WRONG_HWID_WARNING;
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      $('wrong-hwid-message-content').innerHTML =
          '<p>' +
          loadTimeData.getStringF('wrongHWIDMessageFirstPart',
              '<strong>', '</strong>') +
          '</p><p>' +
          loadTimeData.getString('wrongHWIDMessageSecondPart') +
          '</p>';
    },

  };

  return {
    WrongHWIDScreen: WrongHWIDScreen
  };
});

