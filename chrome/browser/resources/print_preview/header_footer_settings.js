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
    this.headerFooterApplies_ = false;
    this.addEventListeners_();
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
      return this.headerFooterApplies_ && this.headerFooterCheckbox_.checked;
    },

    /**
     * Adding listeners to header footer related controls.
     * @private
     */
    addEventListeners_: function() {
      this.headerFooterCheckbox_.onclick =
          this.onHeaderFooterChanged_.bind(this);
      document.addEventListener(customEvents.PDF_LOADED,
                                this.onPDFLoaded_.bind(this));
      document.addEventListener(customEvents.MARGINS_SELECTION_CHANGED,
                                this.onMarginsSelectionChanged_.bind(this));
    },

    onMarginsSelectionChanged_: function(event) {
      this.headerFooterApplies_ = event.selectedMargins !=
          print_preview.MarginSettings.MARGINS_VALUE_NO_MARGINS
      this.setVisible_(this.headerFooterApplies_);
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
     * Listener executing when a |customEvents.PDF_LOADED| event occurs.
     * @private
     */
    onPDFLoaded_: function() {
      if (!previewModifiable)
        this.setVisible_(false);
    },

    /**
     * Hides or shows |this.headerFooterOption|.
     * @{param} visible True if |this.headerFooterOption| should be shown.
     * @private
     */
    setVisible_: function(visible) {
      if (visible)
        fadeInOption(this.headerFooterOption_);
      else
        fadeOutOption(this.headerFooterOption_);
    },
  };

  return {
    HeaderFooterSettings: HeaderFooterSettings,
  };
});
