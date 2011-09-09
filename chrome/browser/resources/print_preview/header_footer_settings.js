// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a HeaderFooterSettings object. This object encapsulates all
   * settings and logic related to the headers and footers checkbox.
   * @constructor
   */
  function HeaderFooterSettings() {
    this.headerFooterOption_ = $('header-footer-option');
    this.headerFooterCheckbox_ = $('header-footer');
  }

  cr.addSingletonGetter(HeaderFooterSettings);

  HeaderFooterSettings.prototype = {
    /**
     * The checkbox corresponding to the headers and footers option.
     * @type {HTMLInputElement}
     */
    get headerFooterCheckbox() {
      return this.headerFooterCheckbox_;
    },

    /**
     * Checks whether the Headers and Footers checkbox is checked or not.
     * @return {boolean} true if Headers and Footers are checked.
     */
    hasHeaderFooter: function() {
      return this.headerFooterCheckbox_.checked;
    },

    /**
     * Adding listeners to header footer related controls.
     */
    addEventListeners: function() {
      this.headerFooterCheckbox_.onclick =
          this.onHeaderFooterChanged_.bind(this);
      document.addEventListener('PDFLoaded', this.onPDFLoaded_.bind(this));
    },

    /**
     * Listener executing when the user selects or de-selects the headers
     * and footers option.
     * @private
     */
    onHeaderFooterChanged_: function() {
      requestPrintPreview();
    },

    /**
     * Listener executing when a PDFLoaded event occurs.
     * @private
     */
    onPDFLoaded_: function() {
      if (!previewModifiable) {
        fadeOutElement(this.headerFooterOption_);
        this.headerFooterCheckbox_.checked = false;
      }
    },
  };

  return {
    HeaderFooterSettings: HeaderFooterSettings,
  };
});
