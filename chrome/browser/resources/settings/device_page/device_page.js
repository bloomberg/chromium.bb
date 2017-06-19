// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-device-page' is the settings page for device and
 * peripheral settings.
 */
Polymer({
  is: 'settings-device-page',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * |hasMouse_| and |hasTouchpad_| start undefined so observers don't trigger
     * until they have been populated.
     * @private
     */
    hasMouse_: Boolean,

    /** @private */
    hasTouchpad_: Boolean,

    /**
     * |hasStylus_| is initialized to false so that dom-if behaves correctly.
     * @private
     */
    hasStylus_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether power status and settings should be fetched and displayed.
     * @private
     */
    enablePowerSettings_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enablePowerSettings');
      },
      readOnly: true,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        var map = new Map();
        map.set(settings.Route.POINTERS.path, '#pointersRow .subpage-arrow');
        map.set(settings.Route.KEYBOARD.path, '#keyboardRow .subpage-arrow');
        map.set(settings.Route.STYLUS.path, '#stylusRow .subpage-arrow');
        map.set(settings.Route.DISPLAY.path, '#displayRow .subpage-arrow');
        map.set(settings.Route.STORAGE.path, '#storageRow .subpage-arrow');
        map.set(settings.Route.POWER.path, '#powerRow .subpage-arrow');
        return map;
      },
    },
  },

  observers: [
    'pointersChanged_(hasMouse_, hasTouchpad_)',
  ],

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'has-mouse-changed', this.set.bind(this, 'hasMouse_'));
    this.addWebUIListener(
        'has-touchpad-changed', this.set.bind(this, 'hasTouchpad_'));
    settings.DevicePageBrowserProxyImpl.getInstance().initializePointers();

    this.addWebUIListener(
        'has-stylus-changed', this.set.bind(this, 'hasStylus_'));
    settings.DevicePageBrowserProxyImpl.getInstance().initializeStylus();
  },

  /**
   * @return {string}
   * @private
   */
  getPointersTitle_: function() {
    if (this.hasMouse_ && this.hasTouchpad_)
      return this.i18n('mouseAndTouchpadTitle');
    if (this.hasMouse_)
      return this.i18n('mouseTitle');
    if (this.hasTouchpad_)
      return this.i18n('touchpadTitle');
    return '';
  },

  /**
   * Handler for tapping the mouse and touchpad settings menu item.
   * @private
   */
  onPointersTap_: function() {
    settings.navigateTo(settings.Route.POINTERS);
  },

  /**
   * Handler for tapping the Keyboard settings menu item.
   * @private
   */
  onKeyboardTap_: function() {
    settings.navigateTo(settings.Route.KEYBOARD);
  },

  /**
   * Handler for tapping the Keyboard settings menu item.
   * @private
   */
  onStylusTap_: function() {
    settings.navigateTo(settings.Route.STYLUS);
  },

  /**
   * Handler for tapping the Display settings menu item.
   * @private
   */
  onDisplayTap_: function() {
    settings.navigateTo(settings.Route.DISPLAY);
  },

  /**
   * Handler for tapping the Storage settings menu item.
   * @private
   */
  onStorageTap_: function() {
    settings.navigateTo(settings.Route.STORAGE);
  },

  /**
   * Handler for tapping the Power settings menu item.
   * @private
   */
  onPowerTap_: function() {
    settings.navigateTo(settings.Route.POWER);
  },

  /** @protected */
  currentRouteChanged: function() {
    this.checkPointerSubpage_();
  },

  /**
   * @param {boolean} hasMouse
   * @param {boolean} hasTouchpad
   * @private
   */
  pointersChanged_: function(hasMouse, hasTouchpad) {
    this.$.pointersRow.hidden = !hasMouse && !hasTouchpad;
    this.checkPointerSubpage_();
  },

  /**
   * Leaves the pointer subpage if all pointing devices are detached.
   * @private
   */
  checkPointerSubpage_: function() {
    // Check that the properties have explicitly been set to false.
    if (this.hasMouse_ === false && this.hasTouchpad_ === false &&
        settings.getCurrentRoute() == settings.Route.POINTERS) {
      settings.navigateTo(settings.Route.DEVICE);
    }
  },
});
