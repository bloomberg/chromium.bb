// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'kerberos-add-account-dialog' is an element to add Kerberos accounts.
 */

Polymer({
  is: 'kerberos-add-account-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * If set, some fields are preset from this account (like username or
     * whether to remember the password).
     * @type {?settings.KerberosAccount}
     */
    presetAccount: Object,

    /** @private */
    username_: {
      type: String,
      value: '',
    },

    /** @private */
    password_: {
      type: String,
      value: '',
    },

    /** @private */
    rememberPassword_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    generalErrorText_: {
      type: String,
      value: '',
    },

    /** @private */
    usernameErrorText_: {
      type: String,
      value: '',
    },

    /** @private */
    passwordErrorText_: {
      type: String,
      value: '',
    },

    /** @private */
    inProgress_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {?settings.KerberosAccountsBrowserProxy} */
  browserProxy_: null,

  /** @private {!settings.KerberosErrorType} */
  lastError_: settings.KerberosErrorType.kNone,

  /** @private {boolean} */
  useRememberedPassword_: false,

  /** @override */
  attached: function() {
    this.$.dialog.showModal();

    if (this.presetAccount) {
      // Preset username and make UI read-only.
      // Note: At least the focus() part needs to be after showModal.
      this.username_ = this.presetAccount.principalName;
      this.$.username.readonly = true;
      this.$.password.focus();

      if (this.presetAccount.hasRememberedPassword) {
        // The daemon knows the user's password, so prefill the password field
        // with some string (Chrome does not know the actual password for
        // security reasons). If the user does not change it, an empty password
        // is sent to the daemon, which is interpreted as "use remembered
        // password". Also, keep remembering the password by default.
        const FAKE_PASSWORD = 'xxxxxxxx';
        this.password_ = FAKE_PASSWORD;
        this.rememberPassword_ = true;
        this.useRememberedPassword_ = true;
      }
    }
  },

  /** @private */
  onCancel_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  onAdd_: function() {
    this.inProgress_ = true;
    this.updateErrorMessages_(settings.KerberosErrorType.kNone);

    // An empty password triggers the Kerberos daemon to use the remembered one.
    const passwordToSubmit = this.useRememberedPassword_ ? '' : this.password_;

    settings.KerberosAccountsBrowserProxyImpl.getInstance()
        .addAccount(this.username_, passwordToSubmit, this.rememberPassword_)
        .then(error => {
          this.inProgress_ = false;

          // Success case. Close dialog.
          if (error == settings.KerberosErrorType.kNone) {
            this.$.dialog.close();
            return;
          }

          // Triggers the UI to update error messages.
          this.updateErrorMessages_(error);
        });
  },

  /** @private */
  onPasswordInput_: function() {
    // On first input, don't reuse the remembered password, but submit the
    // changed one.
    this.useRememberedPassword_ = false;
  },

  /**
   * @param {!settings.KerberosErrorType} error Current error enum
   * @private
   */
  updateErrorMessages_: function(error) {
    this.lastError_ = error;

    this.generalErrorText_ = '';
    this.usernameErrorText_ = '';
    this.passwordErrorText_ = '';

    switch (error) {
      case settings.KerberosErrorType.kNone:
        break;

      case settings.KerberosErrorType.kNetworkProblem:
        this.generalErrorText_ = this.i18n('kerberosErrorNetworkProblem');
        break;
      case settings.KerberosErrorType.kParsePrincipalFailed:
        this.usernameErrorText_ = this.i18n('kerberosErrorUsernameInvalid');
        break;
      case settings.KerberosErrorType.kBadPrincipal:
        this.usernameErrorText_ = this.i18n('kerberosErrorUsernameUnknown');
        break;
      case settings.KerberosErrorType.kContactingKdcFailed:
        this.usernameErrorText_ = this.i18n('kerberosErrorContactingServer');
        break;

      case settings.KerberosErrorType.kBadPassword:
        this.passwordErrorText_ = this.i18n('kerberosErrorPasswordInvalid');
        break;
      case settings.KerberosErrorType.kPasswordExpired:
        this.passwordErrorText_ = this.i18n('kerberosErrorPasswordExpired');
        break;

      case settings.KerberosErrorType.kKdcDoesNotSupportEncryptionType:
        this.generalErrorText_ = this.i18n('kerberosErrorKdcEncType');
        break;
      default:
        this.generalErrorText_ =
            this.i18n('kerberosErrorGeneral', String(error));
    }
  },

  /**
   * Whether an error element should be shown.
   * Note that !! is not supported in Polymer bindings.
   * @param {?string} errorText Error text to be displayed. Empty if no error.
   * @return {boolean} True iff errorText is not empty.
   * @private
   */
  showError_: function(errorText) {
    return !!errorText;
  }
});