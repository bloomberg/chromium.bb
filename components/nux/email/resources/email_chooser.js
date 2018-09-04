// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @const
 */
var nux_email = nux_email || {};

/**
 * @typedef {{
 *    name: string,
 *    icon: string,
 *    url: string,
 *    bookmarkId: {string|undefined},
 * }}
 */
nux_email.EmailProviderModel;

Polymer({
  is: 'email-chooser',
  properties: {
    // TODO(scottchen): get from C++
    /** @private */
    bookmarkBarWasHidden_: Boolean,

    emailList: Array,

    /** @private {nux_email.EmailProviderModel} */
    selectedEmailProvider_: {
      type: Object,
      value: () => null,
      observer: 'onSelectedEmailProviderChange_',
    },
  },

  /** @private {NuxEmailProxy} */
  browserProxy_: null,

  ready: function() {
    this.browserProxy_ = nux.NuxEmailProxyImpl.getInstance();
    this.emailList = this.browserProxy_.getEmailList();
    window.addEventListener('beforeunload', () => {
      this.revertBookmark_();
    });
  },

  /**
   * Handle toggling the email selected.
   * @param {!{model: {item: !nux_email.EmailProviderModel}}} e
   * @private
   */
  onEmailClick_: function(e) {
    if (this.getSelected_(e.model.item))
      this.selectedEmailProvider_ = null;
    else
      this.selectedEmailProvider_ = e.model.item;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onEmailPointerDown_: function(e) {
    e.currentTarget.classList.remove('keyboard-focused');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onEmailKeyUp_: function(e) {
    e.currentTarget.classList.add('keyboard-focused');
  },

  /** @private */
  getSelected_: function(item) {
    return this.selectedEmailProvider_ &&
        item.name === this.selectedEmailProvider_.name;
  },

  /**
   * @param {nux_email.EmailProviderModel=} newEmail
   * @private
   */
  revertBookmark_: function(emailProvider) {
    emailProvider = emailProvider || this.selectedEmailProvider_;

    if (emailProvider && emailProvider.bookmarkId)
      this.browserProxy_.removeBookmark(emailProvider.bookmarkId);

    // TODO: also hide bookmark bar if this.selectedEmailProvider is now null &&
    // the bookmarkBarWasHidden_ == true;
  },

  /**
   * @param {nux_email.EmailProviderModel} newEmail
   * @param {nux_email.EmailProviderModel} prevEmail
   * @private
   */
  onSelectedEmailProviderChange_: function(newEmail, prevEmail) {
    if (prevEmail) {
      // If it was previously selected, it must've been assigned an id.
      assert(prevEmail.bookmarkId);
      this.revertBookmark_(prevEmail);
    }

    if (newEmail) {
      this.browserProxy_.addBookmark(
          {title: newEmail.name, url: newEmail.url, parentId: '1'}, results => {
            this.selectedEmailProvider_.bookmarkId = results.id;
          });
    }
  },

  /** @private */
  onNoThanksClicked_: function() {
    this.revertBookmark_();
    window.location.replace('chrome://newtab');
  },

  /** @private */
  onGetStartedClicked_: function() {
    window.location.replace(this.selectedEmailProvider_.url);
  },
});
