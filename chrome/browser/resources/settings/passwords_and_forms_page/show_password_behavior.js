// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This behavior bundles functionality required to show a password to the user.
 * It is used by both <password-list-item> and <password-edit-dialog>.
 *
 * @polymerBehavior
 */
var ShowPasswordBehavior = {

  properties: {
    /**
     * The password that is being displayed.
     * @type {!chrome.passwordsPrivate.PasswordUiEntry}
     */
    item: Object,

    /**
     * Holds the plaintext password when requested.
     * Initializing it to the empty string is necessary to indicate that the
     * password hasn't been fetched yet.
     */
    password: {
      type: String,
      value: '',
    },
  },

  /**
   * Gets the password input's type. Should be 'text' when password is visible
   * or when there's federated text otherwise 'password'.
   * @private
   */
  getPasswordInputType_: function() {
    return this.password || this.item.federationText ? 'text' : 'password';
  },

  /**
   * Gets the title text for the show/hide icon.
   * @param {string} password
   * @param {string} hide The i18n text to use for 'Hide'
   * @param {string} show The i18n text to use for 'Show'
   * @private
   */
  showPasswordTitle_: function(password, hide, show) {
    return password ? hide : show;
  },

  /**
   * Get the right icon to display when hiding/showing a password.
   * @return {string}
   * @private
   */
  getIconClass_: function() {
    return this.password ? 'icon-visibility-off' : 'icon-visibility';
  },

  /**
   * Gets the text of the password. Will use the value of |password| unless it
   * cannot be shown, in which case it will be spaces. It can also be the
   * federated text.
   * @private
   */
  getPassword_: function() {
    if (!this.item)
      return '';

    return this.item.federationText || this.password ||
        ' '.repeat(this.item.numCharactersInPassword);
  },

  /**
   * Handler for tapping the show/hide button. Will fire an event to request the
   * password for this login pair.
   * @private
   */
  onShowPasswordButtonTap_: function() {
    if (this.password)
      this.password = '';
    else
      this.fire('show-password', this);  // Request the password.
  },
};
