// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays options for exporting the captured data.
 */
var ExportView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function ExportView() {
    assertFirstConstructorCall(ExportView);

    // Call superclass's constructor.
    superClass.call(this, ExportView.MAIN_BOX_ID);

    var securityStrippingCheckbox =
        $(ExportView.SECURITY_STRIPPING_CHECKBOX_ID);
    securityStrippingCheckbox.onclick =
        this.onSetSecurityStripping_.bind(this, securityStrippingCheckbox);

    this.downloadIframe_ = $(ExportView.DOWNLOAD_IFRAME_ID);

    this.saveFileButton_ = $(ExportView.SAVE_FILE_BUTTON_ID);
    this.saveFileButton_.onclick = this.onSaveFile_.bind(this);
    this.saveStatusText_ = $(ExportView.SAVE_STATUS_TEXT_ID);

    this.userCommentsTextArea_ = $(ExportView.USER_COMMENTS_TEXT_AREA_ID);

    // Track blob for previous log dump so it can be revoked when a new dump is
    // saved.
    this.lastBlobURL_ = null;
  }

  // ID for special HTML element in category_tabs.html
  ExportView.TAB_HANDLE_ID = 'tab-handle-export';

  // IDs for special HTML elements in export_view.html
  ExportView.MAIN_BOX_ID = 'export-view-tab-content';
  ExportView.DOWNLOAD_IFRAME_ID = 'export-view-download-iframe';
  ExportView.SAVE_FILE_BUTTON_ID = 'export-view-save-log-file';
  ExportView.SAVE_STATUS_TEXT_ID = 'export-view-save-status-text';
  ExportView.SECURITY_STRIPPING_CHECKBOX_ID =
    'export-view-security-stripping-checkbox';
  ExportView.USER_COMMENTS_TEXT_AREA_ID = 'export-view-user-comments';

  cr.addSingletonGetter(ExportView);

  ExportView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    /**
     * Depending on the value of the checkbox, enables or disables stripping
     * cookies and passwords from log dumps and displayed events.
     */
    onSetSecurityStripping_: function(securityStrippingCheckbox) {
      g_browser.sourceTracker.setSecurityStripping(
          securityStrippingCheckbox.checked);
    },

    onSecurityStrippingChanged: function() {
    },

    /**
     * Called when a log file is loaded, after clearing the old log entries and
     * loading the new ones.  Returns false to indicate the view should be
     * hidden.
     */
    onLoadLogFinish: function(data) {
      return false;
    },

    /**
     * Sets the save to file status text, displayed below the save to file
     * button, to |text|.  Also enables or disables the save button based on the
     * value of |isSaving|, which must be true if the save process is still
     * ongoing, and false when the operation has stopped, regardless of success
     * of failure.
     */
    setSaveFileStatus: function(text, isSaving) {
      this.enableSaveFileButton_(!isSaving);
      this.saveStatusText_.textContent = text;
    },

    enableSaveFileButton_: function(enabled) {
      this.saveFileButton_.disabled = !enabled;
    },

    /**
     * If not already busy saving a log dump, triggers asynchronous
     * generation of log dump and starts waiting for it to complete.
     */
    onSaveFile_: function() {
      if (this.saveFileButton_.disabled)
        return;

      // Get an explanation for the dump file (this is mandatory!)
      var userComments = this.getNonEmptyUserComments_();
      if (userComments == undefined) {
        return;
      }

      // Clean up previous blob, if any, to reduce resource usage.
      if (this.lastBlobURL_) {
        window.webkitURL.revokeObjectURL(this.lastBlobURL_);
        this.lastBlobURL_ = null;
      }
      this.setSaveFileStatus('Preparing data...', true);

      logutil.createLogDumpAsync(userComments,
                                 this.onLogDumpCreated_.bind(this));
    },

    /**
     * Fetches the user comments for this dump. If none were entered, warns the
     * user and returns undefined. Otherwise returns the comments text.
     */
    getNonEmptyUserComments_: function() {
      var value = this.userCommentsTextArea_.value;

      // Reset the class name in case we had hilighted it earlier.
      this.userCommentsTextArea_.className = ''

      // We don't accept empty explanations. We don't care what is entered, as
      // long as there is something (a single whitespace would work).
      if (value == '') {
        // Put a big obnoxious red border around the text area.
        this.userCommentsTextArea_.className =
            'export-view-explanation-warning';
        alert('Please fill in the text field!');
        return undefined;
      }

      return value;
    },

    /**
     * Creates a blob url and starts downloading it.
     */
    onLogDumpCreated_: function(dumpText) {
      var blobBuilder = new WebKitBlobBuilder();
      blobBuilder.append(dumpText, 'native');
      var textBlob = blobBuilder.getBlob('octet/stream');
      this.lastBlobURL_ = window.webkitURL.createObjectURL(textBlob);
      this.downloadIframe_.src = this.lastBlobURL_;
      this.setSaveFileStatus('Dump successful', false);
    }
  };

  return ExportView;
})();
