// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
      return previewModifiable && this.headerFooterCheckbox_.checked;
    },

    /**
     * Sets the state of the headers footers checkbox.
     * @param {boolean} checked True if the headers footers checkbox shoule be
     *     checked, false if not.
     */
    setChecked: function(checked) {
      this.headerFooterCheckbox_.checked = checked;
    },

    /**
     * Checks the printable area and updates the visibility of header footer
     * option based on the selected margins.
     * @param {{contentWidth: number, contentHeight: number, marginLeft: number,
     *          marginRight: number, marginTop: number, marginBottom: number,
     *          printableAreaX: number, printableAreaY: number,
     *          printableAreaWidth: number, printableAreaHeight: number}}
     *          pageLayout Specifies default page layout details in points.
     * @param {number} marginsType Specifies the selected margins type value.
     */
    checkAndHideHeaderFooterOption: function(pageLayout, marginsType) {
      var headerFooterApplies = true;
      if (marginsType ==
              print_preview.MarginSettings.MARGINS_VALUE_NO_MARGINS ||
          !previewModifiable) {
        headerFooterApplies = false;
      } else if (marginsType !=
                     print_preview.MarginSettings.MARGINS_VALUE_MINIMUM) {
        if (cr.isLinux || cr.isChromeOS) {
          headerFooterApplies = pageLayout.marginTop > 0 ||
                                pageLayout.marginBottom > 0;
        } else {
          var pageHeight = pageLayout.marginTop + pageLayout.marginBottom +
                           pageLayout.contentHeight;
          headerFooterApplies =
              (pageLayout.marginTop > pageLayout.printableAreaY) ||
              (pageLayout.marginBottom >
                   (pageHeight - pageLayout.printableAreaY -
                    pageLayout.printableAreaHeight));
        }
      }
      this.setVisible_(headerFooterApplies);
      var headerFooterEvent = new cr.Event(
          customEvents.HEADER_FOOTER_VISIBILITY_CHANGED);
      headerFooterEvent.headerFooterApplies = headerFooterApplies;
      document.dispatchEvent(headerFooterEvent);
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
     * Hides or shows |this.headerFooterOption_|.
     * @param {boolean} visible True if |this.headerFooterOption_| should be
     *     shown.
     * @private
     */
    setVisible_: function(visible) {
        this.headerFooterOption_.style.display = visible ? 'block' : 'none';
    },
  };

  return {
    HeaderFooterSettings: HeaderFooterSettings
  };
});
