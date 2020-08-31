// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-dialog',

  behaviors: [OobeI18nBehavior, CrScrollableBehavior],

  properties: {
    /**
     * Controls visibility of the bottom-buttons element.
     */
    hasButtons: {
      type: Boolean,
      value: false,
    },

    /**
     * Hide the box shadow on the top of oobe-bottom
     */
    hideShadow: {
      type: Boolean,
      value: false,
    },

    /**
     * Control visibility of the header container.
     */
    noHeader: {
      type: Boolean,
      value: false,
    },

    /**
     * Removes footer padding.
     */
    noFooterPadding: {
      type: Boolean,
      value: false,
    },

    /**
     * Removes buttons padding.
     */
    noButtonsPadding: {
      type: Boolean,
      value: false,
    },

    /**
     * True when dialog is displayed in full-screen mode.
     */
    fullScreenDialog: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      observer: 'onfullScreenDialogChanged_',
    },

    /**
     * If true footer would be shrunk as much as possible to fit container.
     */
    footerShrinkable: {
      type: Boolean,
      value: false,
    },

    /* The ID of the localized string to be used as title text when no "title"
     * slot elements are specified.
     */
    titleKey: {
      type: String,
    },

    /* The ID of the localized string to be used as subtitle text when no
     * "subtitle" slot elements are specified.
     */
    subtitleKey: {
      type: String,
    },

    /**
     * If set, prevents lazy instantiation of the dialog.
     */
    noLazy: {
      type: Boolean,
      value: false,
      observer: 'onNoLazyChanged_',
    }

  },

  focus() {
    /* When Network Selection Dialog is shown because user pressed "Back"
       button on EULA screen, display_manager does not inform this dialog that
       it is shown. It ouly focuses this dialog.
       So this emulates show().
       TODO (alemate): fix this once event flow is updated.
    */
    this.show();
  },

  onBeforeShow() {
    this.$$('#lazy').get();
    var isOobe = window.hasOwnProperty('Oobe') &&
        window.hasOwnProperty('DISPLAY_TYPE') && Oobe.getInstance() &&
        Oobe.getInstance().displayType == DISPLAY_TYPE.OOBE;
    if (isOobe || document.documentElement.hasAttribute('full-screen-dialog'))
      this.fullScreenDialog = true;
  },

  /**
   * Scroll to the bottom of footer container.
   */
  scrollToBottom() {
    var el = this.$$('#top-scroll-container');
    el.scrollTop = el.scrollHeight;
  },


  /**
   * Updates the scroll behaviour.
   */
  updateScroll() {
    this.requestUpdateScroll();
  },

  /**
   * This is called from oobe_welcome when this dialog is shown.
   */
  show() {
    var focusedElements = this.getElementsByClassName('focus-on-show');
    var focused = false;
    for (var i = 0; i < focusedElements.length; ++i) {
      if (focusedElements[i].hidden)
        continue;

      focused = true;
      focusedElements[i].focus();
      break;
    }
    if (!focused && focusedElements.length > 0)
      focusedElements[0].focus();

    this.fire('show-dialog');
    this.updateScroll();
  },

  /** @private */
  onfullScreenDialogChanged_() {
    if (this.fullScreenDialog)
      document.documentElement.setAttribute('full-screen-dialog', true);
  },

  /** @private */
  onNoLazyChanged_() {
    if (this.noLazy)
      this.$$('#lazy').get();
  }
});
