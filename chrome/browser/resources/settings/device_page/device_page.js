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
  ],

  properties: {
    /** The current active route. */
    currentRoute: {
      type: Object,
      notify: true,
      observers: 'currentRouteChanged_',
    },

    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    hasMouse_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    hasTouchpad_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    noteAllowed_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('noteAllowed');
      },
      readOnly: true,
    },
  },

  observers: [
    'pointersChanged_(hasMouse_, hasTouchpad_)',
  ],

  ready: function() {
    cr.addWebUIListener('has-mouse-changed', this.set.bind(this, 'hasMouse_'));
    cr.addWebUIListener(
        'has-touchpad-changed', this.set.bind(this, 'hasTouchpad_'));
    settings.DevicePageBrowserProxyImpl.getInstance().initializePointers();
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
   * @return {string}
   * @private
   */
  getPointersIcon_: function() {
    if (this.hasMouse_)
      return 'settings:mouse';
    if (this.hasTouchpad_)
      return 'settings:touch-app';
    return '';
  },

  /**
   * Handler for tapping the mouse and touchpad settings menu item.
   * @private
   */
  onPointersTap_: function() {
    this.$.pages.setSubpageChain(['pointers']);
  },

  /**
   * Handler for tapping the Keyboard settings menu item.
   * @private
   */
  onKeyboardTap_: function() {
    this.$.pages.setSubpageChain(['keyboard']);
  },

  /**
   * Handler for tapping the Keyboard settings menu item.
   * @private
   */
  onNoteTap_: function() {
    this.$.pages.setSubpageChain(['note']);
  },

  /**
   * Handler for tapping the Display settings menu item.
   * @private
   */
  onDisplayTap_: function() {
    this.$.pages.setSubpageChain(['display']);
  },

  /** @private */
  currentRouteChanged_: function() {
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
    if (!this.hasMouse_ && !this.hasTouchpad_ &&
        this.isCurrentRouteOnPointersPage_()) {
      this.$.pages.fire('subpage-back');
    }
  },

  /**
   * TODO(michaelpg): create generic fn for this and isCurrentRouteOnSyncPage_.
   * @return {boolean} Whether the current route shows the pointers page.
   * @private
   */
  isCurrentRouteOnPointersPage_: function() {
    return this.currentRoute &&
        this.currentRoute.page == 'basic' &&
        this.currentRoute.section == 'device' &&
        this.currentRoute.subpage.length == 1 &&
        this.currentRoute.subpage[0] == 'pointers';
  },
});
