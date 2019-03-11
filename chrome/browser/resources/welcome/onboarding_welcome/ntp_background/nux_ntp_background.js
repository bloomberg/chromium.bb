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
    selectedBackground_: {
      observer: 'onSelectedBackgroundChange_',
      type: Object,
    },
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
    if (!this.selectedBackground_) {
      this.selectedBackground_ = defaultBackground;
    }
    if (!this.backgrounds_) {
      this.ntpBackgroundProxy_.getBackgrounds().then((backgrounds) => {
        this.backgrounds_ = [
          defaultBackground,
          ...backgrounds,
        ];
      });
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  hasValidSelectedBackground_: function() {
    return this.selectedBackground_.id > -1;
  },

  /**
   * @param {!nux.NtpBackgroundData} background
   * @private
   */
  isSelectedBackground_: function(background) {
    return background == this.selectedBackground_;
  },

  /** @private */
  onSelectedBackgroundChange_: function() {
    const id = this.selectedBackground_.id;

    if (id > -1) {
      const imageUrl = this.selectedBackground_.imageUrl;
      this.ntpBackgroundProxy_.preloadImage(imageUrl).then(() => {
        if (this.selectedBackground_.id === id) {
          this.$.backgroundPreview.classList.add('active');
          this.$.backgroundPreview.style.backgroundImage = `url(${imageUrl})`;
        }
      });
    } else {
      this.$.backgroundPreview.classList.remove('active');
    }
  },

  /** @private */
  onBackgroundPreviewTransitionEnd_: function() {
    // Whenever the #backgroundPreview transitions to a non-active, hidden
    // state, remove the background image. This way, when the element
    // transitions back to active, the previous background is not displayed.
    if (!this.$.backgroundPreview.classList.contains('active')) {
      this.$.backgroundPreview.style.backgroundImage = '';
    }
  },

  /**
   * @param {!{model: !{item: !nux.NtpBackgroundData}}} e
   * @private
   */
  onBackgroundClick_: function(e) {
    this.selectedBackground_ = e.model.item;
    this.fire('iron-announce', {
      text: this.i18n(
          'ntpBackgroundPreviewUpdated', this.selectedBackground_.title)
    });
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
    if (this.selectedBackground_ && this.selectedBackground_.id > -1) {
      this.ntpBackgroundProxy_.setBackground(this.selectedBackground_.id);
    } else {
      this.ntpBackgroundProxy_.clearBackground();
    }
    welcome.navigateToNextStep();
  },

  /** @private */
  onSkipClicked_: function() {
    welcome.navigateToNextStep();
  },
});
