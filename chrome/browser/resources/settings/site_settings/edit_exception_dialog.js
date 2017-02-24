// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-edit-exception-dialog' is a component for editing a
 * site exception entry.
 */
Polymer({
  is: 'settings-edit-exception-dialog',

  properties: {
    /**
     * @type {!SiteException}
     */
    model: Object,

    /** @private */
    origin_: String,
  },

  /** @private {!settings.SiteSettingsPrefsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.browserProxy_ =
        settings.SiteSettingsPrefsBrowserProxyImpl.getInstance();
    this.origin_ = this.model.origin;

    this.$.dialog.showModal();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.close();
  },

  /** @private */
  onActionButtonTap_: function() {
    if (this.model.origin != this.origin_) {
      // TODO(dpapad): Only COOKIES category can be edited currently,
      // crbug.com/695578.
      var category = settings.ContentSettingsTypes.COOKIES;

      // The way to "edit" an exception is to remove it and and a new one.
      this.browserProxy_.resetCategoryPermissionForOrigin(
          this.model.origin,
          this.model.embeddingOrigin,
          category,
          this.model.incognito);

      this.browserProxy_.setCategoryPermissionForOrigin(
          this.origin_,
          this.origin_,
          category,
          this.model.setting,
          this.model.incognito);
    }

    this.$.dialog.close();
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeypress_: function(e) {
    if (e.key == 'Enter' && !this.$.actionButton.disabled)
      this.onActionButtonTap_();
  },

  /** @private */
  validate_: function() {
    this.browserProxy_.isPatternValid(this.origin_).then(function(isValid) {
      this.$.actionButton.disabled = !isValid;
    }.bind(this));
  },
});
