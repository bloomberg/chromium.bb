// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * QuickUnlockConfigureOverlay class
   * Dialog that allows users to configure screen lock.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function QuickUnlockConfigureOverlay() {
    Page.call(this, 'quickUnlockConfigureOverlay',
              loadTimeData.getString('lockScreenTitle'),
              'quick-unlock-configure-overlay');

  }

  cr.addSingletonGetter(QuickUnlockConfigureOverlay);

  QuickUnlockConfigureOverlay.prototype = {
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);
      $('screen-lock-done').onclick = function() {
        QuickUnlockConfigureOverlay.dismiss();
      };
    },

    /** @override */
    didClosePage: function() {
      settings.navigateTo(settings.Route.PEOPLE);
    },

    /** @override */
    didShowPage: function() {
      this.ensurePolymerIsLoaded_().then(this.onPolymerLoaded_.bind(this));
    },

    /**
     * @return {!Promise}
     * @private
     */
    ensurePolymerIsLoaded_: function() {
      this.loaded_ = this.loaded_ || new Promise(function(resolve) {
        var link = document.createElement('link');
        link.rel = 'import';
        link.href = 'chrome://settings-frame/options_polymer.html';
        link.onload = resolve;
        document.head.appendChild(link);
      });
      return this.loaded_;
    },

    /**
     * Called when polymer has been loaded and the dialog should be displayed.
     * @private
     */
    onPolymerLoaded_: function() {
      settings.navigateTo(settings.Route.LOCK_SCREEN);
      var lockScreen = document.querySelector('settings-lock-screen');

      // On settings the screen lock is part of the lock screen, but on options
      // it is already part of the sync page, so hide the lock screen version on
      // options.
      var screenLockDiv = lockScreen.root.querySelector('#screenLockDiv');
      screenLockDiv.hidden = true;

      // The fingerprint settings on options is always hidden.
      var fingerprintDiv = lockScreen.root.querySelector('#fingerprintDiv');
      fingerprintDiv.hidden = true;

      var passwordPrompt = lockScreen.root.
          querySelector('settings-password-prompt-dialog');
      passwordPrompt.addEventListener('close', function() {
        if (!lockScreen.setModes_) {
          QuickUnlockConfigureOverlay.dismiss();
        }
      }.bind(this));
    },
  };

  QuickUnlockConfigureOverlay.dismiss = function() {
    PageManager.closeOverlay();
  };

  // Export
  return {
    QuickUnlockConfigureOverlay: QuickUnlockConfigureOverlay
  };
});
