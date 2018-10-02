// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @const
 */
var nuxEmail = nuxEmail || {};

/**
 * @typedef {?{
 *    name: string,
 *    icon: string,
 *    url: string,
 *    bookmarkId: (string|undefined),
 * }}
 */
nuxEmail.EmailProviderModel;

Polymer({
  is: 'email-chooser',

  behaviors: [I18nBehavior],

  properties: {
    emailList: Array,

    /** @private */
    bookmarkBarWasShown_: {
      type: Boolean,
      value: loadTimeData.getBoolean('bookmark_bar_shown'),
    },

    /** @private */
    finalized_: Boolean,

    /** @private {nuxEmail.EmailProviderModel} */
    selectedEmailProvider_: {
      type: Object,
      value: () => null,
      observer: 'onSelectedEmailProviderChange_',
    },
  },

  /** @private {nux.NuxEmailProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    Polymer.RenderStatus.afterNextRender(this, function() {
      Polymer.IronA11yAnnouncer.requestAvailability();
    });
  },

  /** @override */
  ready: function() {
    this.browserProxy_ = nux.NuxEmailProxyImpl.getInstance();
    this.browserProxy_.recordPageInitialized();

    this.emailList = this.browserProxy_.getEmailList();

    window.addEventListener('beforeunload', () => {
      // Only need to clean up if user didn't interact with the buttons.
      if (this.finalized_)
        return;

      if (this.selectedEmailProvider_) {
        this.browserProxy_.recordProviderSelected(
            this.selectedEmailProvider_.id);
      }

      this.browserProxy_.recordFinalize();
    });
  },

  /**
   * Handle toggling the email selected.
   * @param {!{model: {item: !nuxEmail.EmailProviderModel}}} e
   * @private
   */
  onEmailClick_: function(e) {
    if (this.getSelected_(e.model.item))
      this.selectedEmailProvider_ = null;
    else
      this.selectedEmailProvider_ = e.model.item;

    this.browserProxy_.recordClickedOption();
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
   * @param {nuxEmail.EmailProviderModel=} emailProvider
   * @private
   */
  revertBookmark_: function(emailProvider) {
    emailProvider = emailProvider || this.selectedEmailProvider_;

    if (emailProvider && emailProvider.bookmarkId)
      this.browserProxy_.removeBookmark(emailProvider.bookmarkId);
  },

  /**
   * @param {nuxEmail.EmailProviderModel} newEmail
   * @param {nuxEmail.EmailProviderModel} prevEmail
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
          {
            title: newEmail.name,
            url: newEmail.url,
            parentId: '1',
          },
          newEmail.id, results => {
            this.selectedEmailProvider_.bookmarkId = results.id;
          });
    } else {
      this.browserProxy_.toggleBookmarkBar(this.bookmarkBarWasShown_);
    }

    // Announcements are mutually exclusive, so keeping separate.
    if (prevEmail && newEmail) {
      this.fire('iron-announce', {text: this.i18n('bookmarkReplaced')});
    } else if (prevEmail) {
      this.fire('iron-announce', {text: this.i18n('bookmarkRemoved')});
    } else if (newEmail) {
      this.fire('iron-announce', {text: this.i18n('bookmarkAdded')});
    }
  },

  /** @private */
  onNoThanksClicked_: function() {
    this.finalized_ = true;
    this.revertBookmark_();
    this.browserProxy_.toggleBookmarkBar(this.bookmarkBarWasShown_);
    this.browserProxy_.recordNoThanks();
    window.location.replace('chrome://newtab');
  },

  /** @private */
  onGetStartedClicked_: function() {
    this.finalized_ = true;
    this.browserProxy_.recordProviderSelected(this.selectedEmailProvider_.id);
    this.browserProxy_.recordGetStarted();
    window.location.replace(this.selectedEmailProvider_.url);
  },

  /** @private */
  onActionButtonClicked_: function() {
    if (this.$$('.action-button').disabled)
      this.browserProxy_.recordClickedDisabledButton();
  },
});