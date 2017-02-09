// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /**
   * Encapsulated handling of the stylus overlay.
   * @constructor
   * @extends {options.SettingsDialog}
   */
  function StylusOverlay() {
    options.SettingsDialog.call(this, 'stylus-overlay',
        loadTimeData.getString('stylusOverlayTabTitle'),
        'stylus-overlay',
        assertInstanceof($('stylus-confirm'), HTMLButtonElement),
        assertInstanceof($('stylus-cancel'), HTMLButtonElement));
  }

  cr.addSingletonGetter(StylusOverlay);

  StylusOverlay.prototype = {
    __proto__: options.SettingsDialog.prototype,

    /**
     * True if the "enable stylus tools" pref is set to true.
     * @type {boolean}
     */
    stylusToolsEnabled_: true,

    /**
     * True if the last call to updateNoteTakingApps_() passed at least one
     * available note-taking app and indicated that we are not waiting for
     * Android to start.
     * @type {boolean}
     */
    noteTakingAppsAvailable_: false,

    /**
     * Note-taking app ID selected by the user, or null if the user didn't
     * change the selection.
     * @type {?string}
     */
    selectedNoteTakingAppId_: null,

    /** @override */
    initializePage: function() {
      options.SettingsDialog.prototype.initializePage.call(this);

      // Disable some elements when enable stylus tools pref is false.
      Preferences.getInstance().addEventListener('settings.enable_stylus_tools',
          function(e) {
            this.stylusToolsEnabled_ = e.value.value;
            $('launch-palette-on-eject-input').disabled = !e.value.value;
            this.updateNoteTakingAppsSelectDisabled_();
          }.bind(this));

      $('stylus-note-taking-app-select')
          .addEventListener(
              'change', this.handleNoteTakingAppSelected_.bind(this));

      var stylusAppsUrl = "https://play.google.com/store/apps/collection/" +
          "promotion_30023cb_stylus_apps";
      $('stylus-find-more-link').onclick = function(event) {
        chrome.send('showPlayStoreApps', [stylusAppsUrl]);
      };
    },

    /** @override */
    handleConfirm: function() {
      options.SettingsDialog.prototype.handleConfirm.call(this);

      if (this.selectedNoteTakingAppId_) {
        chrome.send(
            'setPreferredNoteTakingApp', [this.selectedNoteTakingAppId_]);
        this.selectedNoteTakingAppId_ = null;
      }
    },

    /** @override */
    handleCancel: function() {
      options.SettingsDialog.prototype.handleCancel.call(this);

      var selectOptions = $('stylus-note-taking-app-select').children;
      for (var i = 0; i < selectOptions.length; i++) {
        selectOptions[i].selected = selectOptions[i].defaultSelected;
      }
    },

    /**
     * Updates the list of available note-taking apps. Called from C++.
     * @param {Array<{name: string, id: string, preferred: boolean}>}
     *     apps Array of app info structs containing:
     *       name       App name to display to user.
     *       id         Opaque app ID.
     *       preferred  Whether this is the preferred app.
     * @param {boolean} waitingForAndroid True if Android is starting.
     * @private
     */
    updateNoteTakingApps_: function(apps, waitingForAndroid) {
      var element = $('stylus-note-taking-app-select');

      // Clear any existing options and make sure we don't save an old
      // selection when "OK" is clicked.
      element.textContent = '';
      this.noteTakingAppsAvailable_ = false;
      this.selectedNoteTakingAppId_ = null;

      // Disable the menu and use it to display an informative message if
      // Android is starting or no note-taking apps are installed.
      if (waitingForAndroid) {
        element.appendChild(new Option(
            loadTimeData.getString('stylusNoteTakingAppWaitingForAndroid'), '',
            false, false));
      } else if (apps.length == 0) {
        element.appendChild(new Option(
            loadTimeData.getString('stylusNoteTakingAppNoneAvailable'), '',
            false, false));
      } else {
        for (var i = 0; i < apps.length; i++) {
          var app = apps[i];
          element.appendChild(
              new Option(app.name, app.id, app.preferred, app.preferred));
        }
        this.noteTakingAppsAvailable_ = true;
      }

      this.updateNoteTakingAppsSelectDisabled_();
    },

    /**
     * Updates the disabled state of the note-taking app select element.
     * @private
     */
    updateNoteTakingAppsSelectDisabled_: function() {
      $('stylus-note-taking-app-select').disabled =
          !this.stylusToolsEnabled_ || !this.noteTakingAppsAvailable_;
    },

    /**
     * Records the user's selection of a note-taking app so it can be saved when
     * the "OK" button is clicked.
     * @private
     */
    handleNoteTakingAppSelected_: function() {
      this.selectedNoteTakingAppId_ = $('stylus-note-taking-app-select').value;
    },
  };

  cr.makePublic(StylusOverlay, [
    'updateNoteTakingApps',
  ]);

  // Export
  return {
    StylusOverlay: StylusOverlay
  };
});
