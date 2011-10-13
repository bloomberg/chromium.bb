// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a Margins object that holds four margin values. The units in which
   * the values are expressed can be any numeric value.
   * @constructor
   * @param {number} left The left margin.
   * @param {number} top The top margin.
   * @param {number} right The right margin.
   * @param {number} bottom The bottom margin.
   */
  function Margins(left, top, right, bottom) {
    this[MarginSettings.LEFT_GROUP] = left;
    this[MarginSettings.TOP_GROUP] = top;
    this[MarginSettings.RIGHT_GROUP] = right;
    this[MarginSettings.BOTTOM_GROUP] = bottom;
  }

  /**
   * Rounds |value| keeping |precision| decimal numbers. Example: 32.76643
   * becomes 32.77.
   * @param {number} value The number to round.
   * @param {number} precision The desired precision.
   * @return {number} The rounded number.
   */
  Margins.roundToPrecision = function(value, precision) {
    return Math.round(value * Math.pow(10, precision)) /
        Math.pow(10, precision);
  }

  Margins.prototype = {
    /**
     * Checks if |rhs| is equal to |this|.
     * @param {Margins} rhs The Margins object to compare against.
     * @return {boolean} true if they are equal.
     */
    isEqual: function(rhs) {
      return this[MarginSettings.TOP_GROUP] === rhs[MarginSettings.TOP_GROUP] &&
          this[MarginSettings.LEFT_GROUP] === rhs[MarginSettings.LEFT_GROUP] &&
          this[MarginSettings.RIGHT_GROUP] ===
              rhs[MarginSettings.RIGHT_GROUP] &&
          this[MarginSettings.BOTTOM_GROUP] ===
              rhs[MarginSettings.BOTTOM_GROUP];
    },

    /**
     * Copies the four margin values from |rhs|.
     * @param {Margins} rhs The Margins object values to be used.
     */
    copy: function(rhs) {
      this[MarginSettings.TOP_GROUP] = rhs[MarginSettings.TOP_GROUP];
      this[MarginSettings.LEFT_GROUP] = rhs[MarginSettings.LEFT_GROUP];
      this[MarginSettings.RIGHT_GROUP] = rhs[MarginSettings.RIGHT_GROUP];
      this[MarginSettings.BOTTOM_GROUP] = rhs[MarginSettings.BOTTOM_GROUP];
    },

    /**
     * Converts |this| to inches and returns the result in a new Margins object.
     * |this| is not affected. It assumes that |this| is currently expressed in
     * points.
     * @param {number} The number of decimal points to keep.
     * @return {Margins} The equivalent of |this| in inches.
     */
    toInches: function(precision) {
      return new Margins(
          Margins.roundToPrecision(convertPointsToInches(
              this[MarginSettings.LEFT_GROUP]), precision),
          Margins.roundToPrecision(convertPointsToInches(
              this[MarginSettings.TOP_GROUP]), precision),
          Margins.roundToPrecision(convertPointsToInches(
              this[MarginSettings.RIGHT_GROUP]), precision),
          Margins.roundToPrecision(convertPointsToInches(
              this[MarginSettings.BOTTOM_GROUP]), precision)
      );
    }
  };

  /**
   * @constructor
   * Class describing the layout of the page.
   */
  function PageLayout(width, height, left, top, right, bottom) {
    this.contentWidth_ = width;
    this.contentHeight_ = height;
    this.margins_ = new Margins(left, top, right, bottom);
  }

  PageLayout.prototype = {
    /**
     * @type {number} The width of the page.
     */
    get pageWidth() {
      return this.margins_.left + this.margins_.right + this.contentWidth_;
    },

    /**
     * @type {number} The height of the page.
     */
    get pageHeight() {
      return this.margins_.top + this.margins_.bottom + this.contentHeight_;
    }
  };

  /**
   * Creates a MarginSettings object. This object encapsulates all settings and
   * logic related to the margins mode.
   * @constructor
   */
  function MarginSettings() {
    this.marginsOption_ = $('margins-option');
    this.marginList_ = $('margin-list');
    this.marginsUI_ = null;

    // Holds the custom margin values in points (if set).
    this.customMargins_ = new Margins(-1, -1, -1, -1);
    // Holds the previous custom margin values in points.
    this.previousCustomMargins_ = new Margins(-1, -1, -1, -1);
    // Holds the width of the page in points.
    this.pageWidth_ = -1;
    // Holds the height of the page in points.
    this.pageHeight_ = -1;
    // The last selected margin option.
    this.lastSelectedOption_ = MarginSettings.MARGINS_VALUE_DEFAULT;

    // Holds the currently updated default page layout values.
    this.currentDefaultPageLayout = null;
    // Holds the default page layout values when the custom margins was last
    // selected.
    this.previousDefaultPageLayout_ = null;

    // True if the margins UI should be shown regardless of mouse position.
    this.forceDisplayingMarginLines_ = true;
  }

  // Number of points per inch.
  MarginSettings.POINTS_PER_INCH = 72;
  // Margin list values.
  MarginSettings.MARGINS_VALUE_DEFAULT = 0;
  MarginSettings.MARGINS_VALUE_NO_MARGINS = 1;
  MarginSettings.MARGINS_VALUE_CUSTOM = 2;
  // Default Margins option index.
  MarginSettings.DEFAULT_MARGINS_OPTION_INDEX = 0;
  // Group name corresponding to the top margin.
  MarginSettings.TOP_GROUP = 'top';
  //  Group name corresponding to the left margin.
  MarginSettings.LEFT_GROUP = 'left';
  // Group name corresponding to the right margin.
  MarginSettings.RIGHT_GROUP = 'right';
  // Group name corresponding to the bottom margin.
  MarginSettings.BOTTOM_GROUP = 'bottom';

  cr.addSingletonGetter(MarginSettings);

  MarginSettings.prototype = {
    /**
     * Returns a dictionary of the four custom margin values.
     * @return {object}
     */
    get customMargins() {
      var margins = {};
      margins.marginLeft = this.customMargins_.left;
      margins.marginTop = this.customMargins_.top;
      margins.marginRight = this.customMargins_.right;
      margins.marginBottom = this.customMargins_.bottom;
      return margins;
    },

    /**
     * @return {number} The value of the selected margin option.
     */
    get selectedMarginsValue() {
      var val = this.marginList_.options[this.marginList_.selectedIndex].value;
      return parseInt(val, 10);
    },

    /**
     * @return {boolean} True if default margins are selected.
     */
    isDefaultMarginsSelected: function() {
      return this.selectedMarginsValue == MarginSettings.MARGINS_VALUE_DEFAULT;
    },

    /**
     * @return {boolean} True if no margins are selected.
     */
    isNoMarginsSelected: function() {
      return this.selectedMarginsValue ==
          MarginSettings.MARGINS_VALUE_NO_MARGINS;
    },

    /**
     * @return {boolean} True if custom margins are selected.
     */
    isCustomMarginsSelected: function() {
      return this.selectedMarginsValue == MarginSettings.MARGINS_VALUE_CUSTOM;
    },

    /**
     * If the custom margin values have changed then request a new preview based
     * on the newly set margins.
     * @private
     */
    requestPreviewIfNeeded_: function() {
      if (!this.areMarginSettingsValid())
        return;
      if (this.customMargins_.toInches(2).isEqual(
          this.previousCustomMargins_.toInches(2))) {
        return;
      }
      this.previousCustomMargins_.copy(this.customMargins_);
      setDefaultValuesAndRegeneratePreview(false);
    },

    /**
     * Listener executed when the mouse is over the sidebar. If the custom
     * margin lines are displayed, then, it fades them out.
     * @private
     */
    onSidebarMouseOver_: function(e) {
      if (!this.forceDisplayingMarginLines_)
        this.marginsUI.hide();
    },

    /**
     * Listener executed when the mouse is over the main view. If the custom
     * margin lines are hidden, then, it fades them in.
     * @private
     */
    onMainviewMouseOver_: function() {
      this.forceDisplayingMarginLines_ = false;
      this.marginsUI.show();
    },

    /**
     * Adds listeners to all margin related controls.
     */
    addEventListeners: function() {
      this.marginList_.onchange = this.onMarginsChanged_.bind(this);
      document.addEventListener('PDFLoaded', this.onPDFLoaded_.bind(this));
    },

    /**
     * @return {boolean} True if the margin settings are valid.
     */
    areMarginSettingsValid: function() {
      if (this.marginsUI_ == null)
        return true;

      var pairs = this.marginsUI.pairsAsList;
      return pairs.every(function(pair) { return pair.box_.isValid; });
    },

    /**
     * Calculates the maximum allowable value of the selected margin text for
     * every margin.
     * @return {array} The maximum allowable value in order top, left, right,
     *     bottom.
     * @private
     */
    getMarginValueLimits_: function() {
      var marginValueLimits = [];
      marginValueLimits[0] = this.pageHeight_ - this.customMargins_.bottom;
      marginValueLimits[1] = this.pageWidth_ - this.customMargins_.right;
      marginValueLimits[2] = this.pageWidth_ - this.customMargins_.left;
      marginValueLimits[3] = this.pageHeight_ - this.customMargins_.top;
      return marginValueLimits;
    },

    /**
     * When the user stops typing in the margin text box a new print preview is
     * requested, only if
     *   1) The input is compeletely valid (it can be parsed in its entirety).
     *   2) The newly selected margins differ from the previously selected.
     * @param {cr.Event} event The change event holding information about what
     *     changed.
     * @private
     */
    onMarginTextValueMayHaveChanged_: function(event) {
      var marginBox = event.target;
      var marginBoxValue = convertInchesToPoints(marginBox.margin);
      this.customMargins_[marginBox.marginGroup] = marginBoxValue;
      this.requestPreviewIfNeeded_();
    },

    /**
     * @type {print_preview.MarginsUI} The object holding the UI for specifying
     *     custom margins.
     */
    get marginsUI() {
      if (!this.marginsUI_) {
        this.marginsUI_ = new print_preview.MarginsUI($('mainview'));
        this.marginsUI_.addObserver(
            this.onMarginTextValueMayHaveChanged_.bind(this));
      }
      return this.marginsUI_;
    },

    /**
     * Adds listeners when the custom margins option is selected.
     * @private
     */
    addCustomMarginEventListeners_: function() {
      $('mainview').onmouseover = this.onMainviewMouseOver_.bind(this);
      $('sidebar').onmouseover = this.onSidebarMouseOver_.bind(this);
    },

    /**
     * Removes the event listeners associated with the custom margins option.
     * @private
     */
    removeCustomMarginEventListeners_: function() {
      $('mainview').onmouseover = null;
      $('sidebar').onmouseover = null;
      this.marginsUI.hide();
    },

    /**
     * Updates |this.marginsUI| depending on the specified margins and the
     * position of the page within the plugin.
     * @private
     */
    drawCustomMarginsUI_: function() {
      // TODO(dpapad): find out why passing |!this.areMarginsSettingsValid()|
      // directly produces the opposite value even though
      // |this.getMarginsRectangleInPercent_()| and
      // |this.getMarginValueLimits_()| have no side effects.
      var keepDisplayedValue = !this.areMarginSettingsValid();
      this.marginsUI.update(this.getMarginsRectangleInPercent_(),
                            this.customMargins_,
                            this.getMarginValueLimits_(),
                            keepDisplayedValue);
      this.marginsUI.draw();
    },

    /**
     * Called when there is change in the preview position or size.
     */
    onPreviewPositionChanged: function() {
      if (this.isCustomMarginsSelected() && previewArea.pdfLoaded &&
          pageSettings.totalPageCount != undefined) {
        this.drawCustomMarginsUI_();
      }
    },

    /**
     * Executes when user selects a different margin option, ie,
     * |this.marginList_.selectedIndex| is changed.
     * @private
     */
    onMarginsChanged_: function() {
      if (this.isDefaultMarginsSelected())
        this.onDefaultMarginsSelected_();
      else if (this.isNoMarginsSelected())
        this.onNoMarginsSelected_();
      else if (this.isCustomMarginsSelected())
        this.onCustomMarginsSelected_();

      this.lastSelectedOption_ = this.selectedMarginsValue;
    },

    /**
     * Executes when the default margins option is selected.
     * @private
     */
    onDefaultMarginsSelected_: function() {
      this.removeCustomMarginEventListeners_();
      this.forceDisplayingMarginLines_ = true;
      setDefaultValuesAndRegeneratePreview(false);
    },

    /**
     * Executes when the no margins option is selected.
     * @private
     */
    onNoMarginsSelected_: function() {
      this.removeCustomMarginEventListeners_();
      this.forceDisplayingMarginLines_ = true;
      this.customMargins_ = new Margins(0, 0, 0, 0);
      setDefaultValuesAndRegeneratePreview(false);
    },

    /**
     * Executes when the custom margins option is selected.
     * @private
     */
    onCustomMarginsSelected_: function() {
      this.addCustomMarginEventListeners_();

      if (this.lastSelectedOption_ == MarginSettings.MARGINS_VALUE_DEFAULT)
        this.customMargins_ = this.currentDefaultPageLayout.margins_;
      this.previousCustomMargins_.copy(this.customMargins_);

      if (this.previousDefaultPageLayout_ != this.currentDefaultPageLayout) {
        this.pageWidth_ = this.currentDefaultPageLayout.pageWidth;
        this.pageHeight_ = this.currentDefaultPageLayout.pageHeight;
      }

      this.previousDefaultPageLayout_ = this.currentDefaultPageLayout;
      this.drawCustomMarginsUI_();
      this.marginsUI.show();
    },

    /**
     * Calculates the coordinates of the four margin lines. These are the
     * coordinates where the margin lines should be displayed. The coordinates
     * are expressed in terms of percentages with respect to the total width
     * and height of the plugin.
     * @return {print_preview.Rect} A rectnangle that describes the position of
     *     the four margin lines.
     * @private
     */
    getMarginsRectangleInPercent_: function() {
      var pageLocation = previewArea.getPageLocationNormalized();
      var marginsInPercent = this.getMarginsInPercent_();
      var leftX = pageLocation.x + marginsInPercent.left;
      var topY = pageLocation.y + marginsInPercent.top;
      var contentWidth = pageLocation.width - (marginsInPercent.left +
          marginsInPercent.right);
      var contentHeight = pageLocation.height - (marginsInPercent.top +
          marginsInPercent.bottom);
      return new print_preview.Rect(
          leftX, topY, contentWidth, contentHeight);
    },

    /**
     * @return {print_preview.Margins} The currently selected margin values
     *     normalized to the total width and height of the plugin.
     * @private
     */
    getMarginsInPercent_: function() {
      var pageInformation = previewArea.getPageLocationNormalized();
      var totalWidthInPoints = this.pageWidth_ / pageInformation.width;
      var totalHeightInPoints = this.pageHeight_ / pageInformation.height;
      var marginsInPercent = new Margins(
          this.customMargins_.left / totalWidthInPoints,
          this.customMargins_.top / totalHeightInPoints,
          this.customMargins_.right / totalWidthInPoints,
          this.customMargins_.bottom / totalHeightInPoints);
      return marginsInPercent;
    },

    /**
     * If custom margins is the currently selected option then change to the
     * default margins option.
     * @private
     */
    resetMarginsIfNeeded: function() {
      if (this.isCustomMarginsSelected()) {
        this.marginList_.options[
            MarginSettings.DEFAULT_MARGINS_OPTION_INDEX].selected = true;
        this.removeCustomMarginEventListeners_();
        this.lastSelectedOption_ = MarginSettings.MARGINS_VALUE_DEFAULT;
      }
    },

    /**
     * Executes when a PDFLoaded event occurs.
     * @private
     */
    onPDFLoaded_: function() {
      if (!previewModifiable)
        fadeOutElement(this.marginsOption_);
    }
  };

  return {
    MarginSettings: MarginSettings,
    PageLayout: PageLayout,
  };
});
