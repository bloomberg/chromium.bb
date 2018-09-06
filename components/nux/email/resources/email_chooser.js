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
    emailList: Array,

    /** @private */
    bookmarkBarWasShown_: {
      type: Boolean,
      value: loadTimeData.getBoolean('bookmark_bar_shown'),
    },

    /** @private */
    gotStarted_: Boolean,

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
      // Only need to clean up if user didn't choose "Get Started".
      if (this.gotStarted_)
        return;

      this.revertBookmark_();
      this.browserProxy_.toggleBookmarkBar(this.bookmarkBarWasShown_);
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
  },

  /**
   * @param {nux_email.EmailProviderModel} newEmail
   * @param {nux_email.EmailProviderModel} prevEmail
   * @private
   */
  onSelectedEmailProviderChange_: function(newEmail, prevEmail) {
    if (!this.browserProxy_)
      return;

    if (prevEmail) {
      // If it was previously selected, it must've been assigned an id.
      assert(prevEmail.bookmarkId);
      this.revertBookmark_(prevEmail);
    }

    if (newEmail) {
      this.browserProxy_.toggleBookmarkBar(true);
      this.browserProxy_.addBookmark(
          {title: newEmail.name, url: newEmail.url, parentId: '1'}, results => {
            this.selectedEmailProvider_.bookmarkId = results.id;
          });
    } else {
      this.browserProxy_.toggleBookmarkBar(this.bookmarkBarWasShown_);
    }
  },

  /** @private */
  onNoThanksClicked_: function() {
    window.location.replace('chrome://newtab');
  },

  /** @private */
  onGetStartedClicked_: function() {
    this.gotStarted_ = true;
    window.location.replace(this.selectedEmailProvider_.url);
  },
});