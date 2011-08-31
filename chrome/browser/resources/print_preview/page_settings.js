// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a PageSettings object. This object encapsulates all settings and
   * logic related to page selection.
   * @constructor
   */
  function PageSettings() {
    this.allPagesRadioButton_ = $('all-pages');
    this.selectedPagesRadioButton_ = $('print-pages');
    this.selectedPagesTextfield_ = $('individual-pages');
    this.selectedPagesHint_ = $('individual-pages-hint');

    // Timer id of |this.selectedPagesTextfield|. It is used to reset the timer
    // whenever needed.
    this.timerId_;

    // Contains the previously selected pages (pages requested by last
    // preview request). It is used in
    // |this.onSelectedPagesMayHaveChanged_()| to make sure that a new preview
    // is not requested more often than necessary.
    this.previouslySelectedPages_ = [];

    // The total page count of the previewed document regardless of which pages
    // the user has selected.
    this.totalPageCount_ = undefined;
  }

  cr.addSingletonGetter(PageSettings);

  PageSettings.prototype = {
    /**
     * The text that is currently in |this.selectedPagesTextfield|.
     * @type {string}
     */
    get selectedPagesText() {
      return this.selectedPagesTextfield_.value;
    },

    /**
     * The radio button corresponding to "all pages selected".
     * @type {HTMLInputElement}
     */
    get allPagesRadioButton() {
      return this.allPagesRadioButton_;
    },

    /**
     * The radio button corresponding to "specific pages selected".
     * @type {HTMLInputElement}
     */
    get selectedPagesRadioButton() {
      return this.selectedPagesRadioButton_;
    },

    /**
     * The textfield containing page ranges as specified by the user.
     * @type {HTMLInputElement}
     */
    get selectedPagesTextfield() {
      return this.selectedPagesTextfield_;
    },

    /**
     * The span element containing the hint shown to the user when page
     * selection is not valid.
     * @type {HTMLElement}
     */
    get selectedPagesHint() {
      return this.selectedPagesHint_;
    },

    /**
     * The total page count of the previewed document regardless of which pages
     * the user has selected. If the total count is not known this value must
     * be undefined.
     * @type {number}
     */
    get totalPageCount() {
      return this.totalPageCount_;
    },

    /**
     * @param {number} count The number to assing to |this.totalPageCount_|.
     */
    set totalPageCount(count) {
      this.totalPageCount_ = count;
    },

    /**
     * Returns the selected pages in ascending order without any duplicates.
     *
     * @return {Array}
     */
    get selectedPagesSet() {
      var selectedPagesText = this.selectedPagesText;

      if (this.allPagesRadioButton.checked || selectedPagesText.length == 0)
        selectedPagesText = '1-' + this.totalPageCount_;

      var pageList = pageRangeTextToPageList(selectedPagesText,
                                             this.totalPageCount_);
      return pageListToPageSet(pageList);
    },

    /**
     * Returns the previously selected pages in ascending order without any
     * duplicates.
     *
     * @return {Array}
     */
    get previouslySelectedPages() {
      return this.previouslySelectedPages_;
    },

    /**
     * Returns an array of objects describing the selected page ranges. See
     * documentation of pageSetToPageRanges() for more details.
     * @return {Array}
     */
    get selectedPageRanges() {
      return pageSetToPageRanges(this.selectedPagesSet);
    },

    /**
     * Invalidates |this.totalPageCount_| to indicate that the total number of
     * pages is not known.
     * @private
     */
    invalidateTotalPageCount_: function() {
      this.totalPageCount_ = undefined;
    },

    /**
     * Invlidates |this.previouslySelectedPages_| to indicate that this value
     * does no longer apply.
     * @private
     */
    invalidatePreviouslySelectedPages_: function() {
      this.previouslySelectedPages_.length = 0;
    },

    /**
     * Resets all the state variables of this object and hides
     * |this.selectedPagesHint|.
     */
    resetState: function() {
      this.selectedPagesTextfield.classList.remove('invalid');
      fadeOutElement(this.selectedPagesHint_);
      this.invalidateTotalPageCount_();
      this.invalidatePreviouslySelectedPages_();
    },

    /**
     * Updates |this.totalPageCount_| and |this.previouslySelectedPages_|,
     * only if they have been previously invalidated.
     * @param {number} newTotalPageCount The new total page count.
     */
    updateState: function(newTotalPageCount) {
      if (!this.totalPageCount_)
        this.totalPageCount_ = newTotalPageCount;

      if (this.previouslySelectedPages_.length == 0) {
        for (var i = 0; i < this.totalPageCount_; i++)
          this.previouslySelectedPages_.push(i + 1);
      }

      if (!this.isPageSelectionValid())
        this.onSelectedPagesTextfieldChanged();
    },

    /**
     * Updates |this.previouslySelectedPages_| with the currently selected
     * pages.
     */
    updatePageSelection: function() {
      this.previouslySelectedPages_ = this.selectedPagesSet;
    },

    /**
     * @private
     * @return {boolean} true if currently selected pages differ from
     *     |this.previouslySelectesPages_|.
     */
    hasPageSelectionChanged_: function() {
      return !areArraysEqual(this.previouslySelectedPages_,
                             this.selectedPagesSet);
    },

    /**
     * Checks if the page selection has changed and is valid.
     * @return {boolean} true if the page selection is changed and is valid.
     */
    hasPageSelectionChangedAndIsValid: function() {
      return this.isPageSelectionValid() && this.hasPageSelectionChanged_();
    },

    /**
     * Validates the contents of |this.selectedPagesTextfield|.
     *
     * @return {boolean} true if the text is valid.
     */
    isPageSelectionValid: function() {
      if (this.allPagesRadioButton_.checked ||
          this.selectedPagesText.length == 0) {
        return true;
      }
      return isPageRangeTextValid(this.selectedPagesText, this.totalPageCount_);
    },

    /**
     * Checks all page selection related settings and requests a new print
     * previw if needed.
     * @return {boolean} true if a new preview was requested.
     */
    requestPrintPreviewIfNeeded: function() {
      if (this.hasPageSelectionChangedAndIsValid()) {
        this.updatePageSelection();
        requestPrintPreview();
        return true;
      }
      if (!this.isPageSelectionValid())
        this.onSelectedPagesTextfieldChanged();
      return false;
    },

    /**
     * Validates the selected pages and updates the hint accordingly.
     * @private
     */
    validateSelectedPages_: function() {
      if (this.isPageSelectionValid()) {
        this.selectedPagesTextfield.classList.remove('invalid');
        fadeOutElement(this.selectedPagesHint);
        this.selectedPagesHint.setAttribute('aria-hidden', 'true');
      } else {
        this.selectedPagesTextfield.classList.add('invalid');
        this.selectedPagesHint.classList.remove('suggestion');
        this.selectedPagesHint.setAttribute('aria-hidden', 'false');
        this.selectedPagesHint.innerHTML =
            localStrings.getStringF('pageRangeInstruction',
                                    localStrings.getString(
                                        'examplePageRangeText'));
        fadeInElement(this.selectedPagesHint);
      }
    },

    /**
     * Executes whenever a blur event occurs on |this.selectedPagesTextfield|
     * or when the timer expires. It takes care of
     * 1) showing/hiding warnings/suggestions
     * 2) updating print button/summary
     */
    onSelectedPagesTextfieldChanged: function() {
      this.validateSelectedPages_();
      cr.dispatchSimpleEvent(document, 'updateSummary');
      cr.dispatchSimpleEvent(document, 'updatePrintButton');
    },

    /**
     * When the user stops typing in |this.selectedPagesTextfield| or clicks on
     * |allPagesRadioButton|, a new print preview is requested, only if
     * 1) The input is compeletely valid (it can be parsed in its entirety).
     * 2) The newly selected pages differ from |this.previouslySelectedPages_|.
     * @private
     */
    onSelectedPagesMayHaveChanged_: function() {
      if (this.selectedPagesRadioButton_.checked)
        this.onSelectedPagesTextfieldChanged();

      // Toggling between "all pages"/"some pages" radio buttons while having an
      // invalid entry in the page selection textfield still requires updating
      // the print summary and print button.
      if (!this.isPageSelectionValid() || !this.hasPageSelectionChanged_()) {
        cr.dispatchSimpleEvent(document, 'updateSummary');
        cr.dispatchSimpleEvent(document, 'updatePrintButton');
        return;
      }
      requestPrintPreview();
    },

    /**
     * Whenever |this.selectedPagesTextfield| gains focus we add a timer to
     * detect when the user stops typing in order to update the print preview.
     * @private
     */
    addTimerToSelectedPagesTextfield_: function() {
      this.timerId_ = window.setTimeout(
          this.onSelectedPagesMayHaveChanged_.bind(this), 1000);
    },

    /**
     * As the user types in |this.selectedPagesTextfield|, we need to reset
     * this timer, since the page ranges are still being edited.
     * @private
     */
    resetSelectedPagesTextfieldTimer_: function() {
      clearTimeout(this.timerId_);
      this.addTimerToSelectedPagesTextfield_();
    },

    /**
     * Handles the blur event of |this.selectedPagesTextfield|.
     * @private
     */
    onSelectedPagesTextfieldBlur_: function() {
      this.selectedPagesRadioButton.checked = true;
      this.validateSelectedPages_();
      cr.dispatchSimpleEvent(document, 'updatePrintButton');
    },

    /**
     * Gives focus to |this.selectedPagesTextfield| when
     * |this.selectedPagesRadioButton| is clicked.
     * @private
     */
    onSelectedPagesRadioButtonChecked_: function() {
      this.selectedPagesTextfield_.focus();
    },

    /**
     * Listener executing when an input event occurs in
     * |this.selectedPagesTextfield|. Ensures that
     * |this.selectedPagesTextfield| is non-empty before checking
     * |this.selectedPagesRadioButton|.
     * @private
     */
    onSelectedPagesTextfieldInput_: function() {
      if (this.selectedPagesText.length)
        this.selectedPagesRadioButton.checked = true;
      this.resetSelectedPagesTextfieldTimer_();
    },

    /**
     * Adding listeners to all pages related controls. The listeners take care
     * of altering their behavior depending on |hasPendingPreviewRequest|.
     */
    addEventListeners: function() {
      this.allPagesRadioButton.onclick =
          this.onSelectedPagesMayHaveChanged_.bind(this);
      this.selectedPagesRadioButton.onclick =
          this.onSelectedPagesMayHaveChanged_.bind(this);
      this.selectedPagesTextfield.oninput =
          this.onSelectedPagesTextfieldInput_.bind(this);
      this.selectedPagesTextfield.onfocus =
          this.addTimerToSelectedPagesTextfield_.bind(this);

      // Handler for the blur event on |this.selectedPagesTextfield|. Un-checks
      // |this.selectedPagesRadioButton| if the input field is empty.
      this.selectedPagesTextfield.onblur = function() {
        if (!this.selectedPagesText.length)
          this.allPagesRadioButton_.checked = true;

        clearTimeout(this.timerId_);
        this.onSelectedPagesMayHaveChanged_();
      }.bind(this);
    }
  };

  return {
    PageSettings: PageSettings,
  };
});
