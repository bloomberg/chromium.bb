// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const KEYBOARD_FOCUSED_CLASS = 'keyboard-focused';

Polymer({
  is: 'nux-ntp-background',

  behaviors: [
    I18nBehavior,
    welcome.NavigationBehavior,
  ],

  properties: {
    /** @type {nux.stepIndicatorModel} */
    indicatorModel: Object,

    /** @private {?nux.NtpBackgroundData} */
    selectedBackground_: Object,
  },

  /** @private {?Array<!nux.NtpBackgroundData>} */
  backgrounds_: null,

  /** @private {?nux.NtpBackgroundProxy} */
  ntpBackgroundProxy_: null,

  /** @override */
  ready: function() {
    this.ntpBackgroundProxy_ = nux.NtpBackgroundProxyImpl.getInstance();
  },

  onRouteEnter: function() {
    const defaultBackground = {
      id: -1,
      imageUrl: '',
      thumbnailClass: '',
      title: this.i18n('ntpBackgroundDefault'),
    };
    this.selectedBackground_ = defaultBackground;
    this.ntpBackgroundProxy_.getBackgrounds().then((backgrounds) => {
      this.backgrounds_ = [
        defaultBackground,
        ...backgrounds,
      ];
    });
  },

  /**
   * @param {!nux.NtpBackgroundData} background
   * @private
   */
  getAriaPressedValue_: function(background) {
    return this.isSelectedBackground_(background).toString();
  },

  /**
   * @param {!nux.NtpBackgroundData} background
   * @private
   */
  isSelectedBackground_: function(background) {
    return background == this.selectedBackground_;
  },

  /**
   * @param {!{model: !{item: !nux.NtpBackgroundData}}} e
   * @private
   */
  onBackgroundClick_: function(e) {
    this.selectedBackground_ = e.model.item;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onBackgroundKeyUp_: function(e) {
    if (e.key == 'ArrowRight' || e.key == 'ArrowDown') {
      this.changeFocus_(e.currentTarget, 1);
    } else if (e.key == 'ArrowLeft' || e.key == 'ArrowUp') {
      this.changeFocus_(e.currentTarget, -1);
    } else {
      this.changeFocus_(e.currentTarget, 0);
    }
  },

  /**
   * @param {EventTarget} element
   * @param {number} direction
   * @private
   */
  changeFocus_(element, direction) {
    if (isRTL()) {
      direction *= -1;  // Reverse direction if RTL.
    }

    const buttons = this.root.querySelectorAll('.ntp-background-grid-button');
    const targetIndex = Array.prototype.indexOf.call(buttons, element);

    const oldFocus = buttons[targetIndex];
    if (!oldFocus) {
      return;
    }

    const newFocus = buttons[targetIndex + direction];

    if (newFocus && direction) {
      newFocus.classList.add(KEYBOARD_FOCUSED_CLASS);
      oldFocus.classList.remove(KEYBOARD_FOCUSED_CLASS);
      newFocus.focus();
    } else {
      oldFocus.classList.add(KEYBOARD_FOCUSED_CLASS);
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onBackgroundPointerDown_: function(e) {
    e.currentTarget.classList.remove(KEYBOARD_FOCUSED_CLASS);
  },

  /** @private */
  onNextClicked_: function() {
    welcome.navigateToNextStep();
  },

  /** @private */
  onSkipClicked_: function() {
    welcome.navigateToNextStep();
  },
});
