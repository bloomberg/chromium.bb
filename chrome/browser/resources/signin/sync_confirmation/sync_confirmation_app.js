/* Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

Polymer({
  is: 'sync-confirmation-app',

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private */
    accountImageSrc_: {
      type: String,
      value: function() {
        return loadTimeData.getString('accountPictureUrl');
      },
    },
  },

  /** @private {?sync.confirmation.SyncConfirmationBrowserProxy} */
  syncConfirmationBrowserProxy_: null,

  /** @private {?function(Event)} */
  boundKeyDownHandler_: null,

  /** @override */
  attached: function() {
    this.syncConfirmationBrowserProxy_ =
        sync.confirmation.SyncConfirmationBrowserProxyImpl.getInstance();
    this.boundKeyDownHandler_ = this.onKeyDown_.bind(this);
    // This needs to be bound to document instead of "this" because the dialog
    // window opens initially, the focus level is only on document, so the key
    // event is not captured by "this".
    document.addEventListener('keydown', this.boundKeyDownHandler_);
    this.addWebUIListener(
        'account-image-changed', this.handleAccountImageChanged_.bind(this));
    this.syncConfirmationBrowserProxy_.requestAccountImage();
  },

  /** @override */
  detached: function() {
    document.removeEventListener('keydown', this.boundKeyDownHandler_);
  },

  /** @private */
  onConfirm_: function(e) {
    this.syncConfirmationBrowserProxy_.confirm(
        this.getConsentDescription_(), this.getConsentConfirmation_(e.path));
  },

  /** @private */
  onUndo_: function() {
    this.syncConfirmationBrowserProxy_.undo();
  },

  /** @private */
  onGoToSettings_: function(e) {
    this.syncConfirmationBrowserProxy_.goToSettings(
        this.getConsentDescription_(), this.getConsentConfirmation_(e.path));
  },

  /** @private */
  onKeyDown_: function(e) {
    if (e.key == 'Enter' && !/^(A|CR-BUTTON)$/.test(e.path[0].tagName)) {
      this.onConfirm_(e);
      e.preventDefault();
    }
  },

  /**
   * @param {!Array<!HTMLElement>} path Path of the click event. Must contain
   *     a consent confirmation element.
   * @return {string} The text of the consent confirmation element.
   * @private
   */
  getConsentConfirmation_: function(path) {
    for (const element of path) {
      if (element.nodeType !== Node.DOCUMENT_FRAGMENT_NODE &&
          element.hasAttribute('consent-confirmation')) {
        return element.innerHTML.trim();
      }
    }
    assertNotReached('No consent confirmation element found.');
    return '';
  },

  /** @return {!Array<string>} Text of the consent description elements. */
  getConsentDescription_: function() {
    const consentDescription =
        Array.from(this.shadowRoot.querySelectorAll('[consent-description]'))
            .filter(element => element.clientWidth * element.clientHeight > 0)
            .map(element => element.innerHTML.trim());
    assert(consentDescription);
    return consentDescription;
  },

  /**
   * Called when the account image changes.
   * @param {string} imageSrc
   * @private
   */
  handleAccountImageChanged_: function(imageSrc) {
    this.accountImageSrc_ = imageSrc;
  },

});
