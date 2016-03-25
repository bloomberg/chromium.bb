// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview settings-certificate-subentry represents an SSL certificate
 * sub-entry.
 */

/**
 * The payload of the certificate-action event that is emitted from this
 * component.
 * @typedef {{
 *   action: !settings.CertificateAction,
 *   subnode: !CertificateSubnode,
 *   certificateType: !settings.CertificateType
 * }}
 */
var CertificateActionEventDetail;

cr.define('settings', function() {
  /**
   * Enumeration of actions that require a popup menu to be shown to the user.
   * @enum {number}
   */
  var CertificateAction = {
    DELETE: 0,
    EDIT: 1,
    EXPORT_PERSONAL: 2,
    IMPORT_CA: 3,
    IMPORT_PERSONAL: 4,
  };

  /**
   * The name of the event that is fired when a menu item is tapped.
   * @type {string}
   */
  var CertificateActionEvent = 'certificate-action';

  return {
    CertificateAction: CertificateAction,
    CertificateActionEvent: CertificateActionEvent,
  };
});

Polymer({
  is: 'settings-certificate-subentry',

  properties: {
    /** @type {!CertificateSubnode>} */
    model: Object,

    /** @type {!settings.CertificateType} */
    certificateType: String,
  },

  /** @private {!settings.CertificatesManagerBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.CertificatesBrowserProxyImpl.getInstance();
  },

  /**
   * Dispatches an event indicating which certificate action was tapped. It is
   * used by the parent of this element to display a modal dialog accordingly.
   * @param {!settings.CertificateAction} action
   * @private
   */
  dispatchCertificateActionEvent_: function(action) {
    this.fire(
        settings.CertificateActionEvent,
        /** @type {!CertificateActionEventDetail} */ ({
          action: action,
          subnode: this.model,
          certificateType: this.certificateType,
        }));
  },

  /**
   * Handles the case where a call to the browser resulted in a rejected
   * promise.
   * @param {null|!CertificatesError|!CertificatesImportError} error
   * @private
   */
  onRejected_: function(error) {
    if (error === null) {
      // Nothing to do here. Null indicates that the user clicked "cancel" on
      // the native file chooser dialog.
      return;
    }

    // Otherwise propagate the error to the parents, such that a dialog
    // displaying the error will be shown.
    this.fire('certificates-error', error);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onViewTap_: function(event) {
    this.closePopupMenu_();
    this.browserProxy_.viewCertificate(this.model.id);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEditTap_: function(event) {
    this.closePopupMenu_();
    this.dispatchCertificateActionEvent_(settings.CertificateAction.EDIT);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onDeleteTap_: function(event) {
    this.closePopupMenu_();
    this.dispatchCertificateActionEvent_(settings.CertificateAction.DELETE);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onExportTap_: function(event) {
    this.closePopupMenu_();
    if (this.certificateType == settings.CertificateType.PERSONAL) {
      this.browserProxy_.exportPersonalCertificate(this.model.id).then(
          function() {
            this.dispatchCertificateActionEvent_(
                settings.CertificateAction.EXPORT_PERSONAL);
          }.bind(this),
          this.onRejected_.bind(this));
    } else {
      this.browserProxy_.exportCertificate(this.model.id);
    }
  },

  /** @private */
  onImportTap_: function() {
    this.closePopupMenu_();
    if (this.certificateType == settings.CertificateType.PERSONAL) {
      // TODO(dpapad): Figure out when to pass true (ChromeOS?).
      this.browserProxy_.importPersonalCertificate(false).then(
          function(showPasswordPrompt) {
            if (showPasswordPrompt) {
              this.dispatchCertificateActionEvent_(
                  settings.CertificateAction.IMPORT_PERSONAL);
            }
          }.bind(this),
          this.onRejected_.bind(this));
    } else if (this.certificateType == settings.CertificateType.CA) {
      this.browserProxy_.importCaCertificate().then(
          function(certificateName) {
            this.dispatchCertificateActionEvent_(
                settings.CertificateAction.IMPORT_CA);
          }.bind(this),
          this.onRejected_.bind(this));
    } else if (this.certificateType == settings.CertificateType.SERVER) {
      this.browserProxy_.importServerCertificate().catch(
          this.onRejected_.bind(this));
    }
  },

  /**
   * @param {string} certificateType The type of this certificate.
   * @return {boolean} Whether the certificate can be edited.
   * @private
   */
  canEdit_: function(certificateType) {
    return this.certificateType == settings.CertificateType.CA;
  },

  /**
   * @param {string} certificateType The type of this certificate.
   * @return {boolean} Whether a certificate can be imported.
   * @private
   */
  canImport_: function(certificateType) {
    return this.certificateType != settings.CertificateType.OTHER;
  },

  /** @private */
  closePopupMenu_: function() {
    this.$$('iron-dropdown').close();
  },
});
