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
      if (!rhs)
        return false;
      return this[MarginSettings.TOP_GROUP] === rhs[MarginSettings.TOP_GROUP] &&
          this[MarginSettings.LEFT_GROUP] === rhs[MarginSettings.LEFT_GROUP] &&
          this[MarginSettings.RIGHT_GROUP] ===
              rhs[MarginSettings.RIGHT_GROUP] &&
          this[MarginSettings.BOTTOM_GROUP] ===
              rhs[MarginSettings.BOTTOM_GROUP];
    },

    clone: function() {
      return new Margins(this[MarginSettings.LEFT_GROUP],
                         this[MarginSettings.TOP_GROUP],
                         this[MarginSettings.RIGHT_GROUP],
                         this[MarginSettings.BOTTOM_GROUP]);
    },

    /**
     * Helper method returning an array of the string indices used for accessing
     * all margins.
     * @return {array} An array of string indices.
     * @private
     */
    indicesAsArray_: function() {
      return [MarginSettings.LEFT_GROUP, MarginSettings.TOP_GROUP,
          MarginSettings.RIGHT_GROUP, MarginSettings.BOTTOM_GROUP];
    },

    /**
     * Rounds |this| based on the precision used when displaying the margins in
     * the user's prefered units. This is done by converting from points to
     * those units (mm/inches) and back to points.
     */
    roundToLocaleUnits: function() {
      var indicesAsArray = this.indicesAsArray_();
      for (var i = 0; i < indicesAsArray.length; i++) {
        this[indicesAsArray[i]] =
            print_preview.convertPointsToLocaleUnitsAndBack(
                this[indicesAsArray[i]]);
      }
    },
  };

  /**
   * @constructor
   * Class describing the layout of the page.
   */
  function PageLayout(width, height, left, top, right, bottom) {
    this.contentWidth_ = width;
    this.contentHeight_ = height;
    this.margins_ = new Margins(left, top, right, bottom);
    this.margins_.roundToLocaleUnits();
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
    this.customMargins_ = null;
    // Holds the previous custom margin values in points.
    this.previousCustomMargins_ = null;
    // Holds the width of the page in points.
    this.pageWidth_ = -1;
    // Holds the height of the page in points.
    this.pageHeight_ = -1;
    // @type {boolean} True if the margins UI should be diplayed when the next
    //     |customEvents.PDF_LOADED| event occurs.
    this.forceMarginsUIOnPDFLoad_ = false;
    // Holds the currently updated default page layout values.
    this.currentDefaultPageLayout = null;

    // True if the margins UI should be shown regardless of mouse position.
    this.forceDisplayingMarginLines_ = true;

    // @type {EventTracker} Used to keep track of certain event listeners.
    this.eventTracker_ = new EventTracker();

    this.addEventListeners_();
  }

  // Number of points per inch.
  MarginSettings.POINTS_PER_INCH = 72;
  // Minimum allowed distance in points between top-bottom, left-right margins.
  MarginSettings.MINIMUM_MARGINS_DISTANCE = 36;
  // Margin list values. Matches enum MarginType in
  // printing/print_job_constants.h.
  MarginSettings.MARGINS_VALUE_DEFAULT = 0;
  MarginSettings.MARGINS_VALUE_NO_MARGINS = 1;
  MarginSettings.MARGINS_VALUE_MINIMUM = 2;
  MarginSettings.MARGINS_VALUE_CUSTOM = 3;
  // Default Margins option index.
  MarginSettings.OPTION_INDEX_DEFAULT = 0;
  // Group name corresponding to the top margin.
  MarginSettings.TOP_GROUP = 'top';
  //  Group name corresponding to the left margin.
  MarginSettings.LEFT_GROUP = 'left';
  // Group name corresponding to the right margin.
  MarginSettings.RIGHT_GROUP = 'right';
  // Group name corresponding to the bottom margin.
  MarginSettings.BOTTOM_GROUP = 'bottom';

  /**
   * Extracts the number formatting and measurement system for the current
   * locale.
   * @param {string} numberFormat Is the formatted version of a sample number,
   *     sent from the backend.
   * @param {number} measurementSystem 0 for SI (aka metric system), 1 for the
   *     system used in the US. Note: Mathces UMeasurementSystem enum in
   *     third_party/icu/public/i18n/unicode/ulocdata.h.
   */
  MarginSettings.setNumberFormatAndMeasurementSystem = function(
      numberFormat, measurementSystem) {
    var regex = /^(\d+)(\.|\,)(\d+)(\.|\,)(\d+)$/;
    var matches = numberFormat.match(regex);
    MarginSettings.thousandsPoint = matches[2];
    MarginSettings.decimalPoint = matches[4];
    MarginSettings.useMetricSystem = measurementSystem == 0;
  };

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
     * Sets |this.customMargins_| according to |margins|.
     * @param {{marginLeft: number, marginTop: number, marginRight: number,
     *     marginBottom: number}} margins An object holding the four margin
     *     values.
     */
    set customMargins(margins) {
      this.customMargins_.left = margins.marginLeft;
      this.customMargins_.top = margins.marginTop;
      this.customMargins_.right = margins.marginRight;
      this.customMargins_.bottom = margins.marginBottom;
    },

    /**
     * @return {number} The value of the selected margin option.
     */
    get selectedMarginsValue() {
      var val = this.marginList_.options[this.marginList_.selectedIndex].value;
      return parseInt(val, 10);
    },

    /**
     * Sets the current margin selection to |lastUsedMarginType|.
     * @param {number} lastUsedMarginType An integer value identifying a margin
     *     type according to MarginType enum in printing/print_job_constants.h.
     * @param {Object} lastUsedCustomMargins The last used custom margins. If
     *     custom margins have not been used before
     *     |margin{Top|Bottom|Left|Right}| attributes are missing.
     */
    setLastUsedMargins: function(lastUsedMarginsSettings) {
      var lastUsedMarginsType = lastUsedMarginsSettings['marginsType'];
      this.forceMarginsUIOnPDFLoad_ =
          lastUsedMarginsType == MarginSettings.MARGINS_VALUE_CUSTOM;
      this.marginList_.selectedIndex =
          this.getMarginOptionIndexByValue_(lastUsedMarginsType);
      if (lastUsedMarginsSettings.hasOwnProperty('marginTop') &&
          lastUsedMarginsSettings.hasOwnProperty('marginBottom') &&
          lastUsedMarginsSettings.hasOwnProperty('marginRight') &&
          lastUsedMarginsSettings.hasOwnProperty('marginLeft')) {
        this.customMargins_ = new Margins(-1, -1, -1 , -1);
        this.customMargins = lastUsedMarginsSettings;
      }
    },

    /**
     * @return {number} The total width of the plugin in points.
     */
    get totalWidthInPoints() {
      var pageInformation = previewArea.pageLocationNormalized;
      return this.pageWidth_ / pageInformation.width;
    },

    /**
     * @return {number} The total height of the plugin in points.
     */
    get totalHeightInPoints() {
      var pageInformation = previewArea.pageLocationNormalized;
      return this.pageHeight_ / pageInformation.height;
    },

    /**
     * Maps margin type values to indices within |this.marginList_|.
     * @param {number} marginTypeValue An integer value identifying a margin
     *     type according to MarginType enum in printing/print_job_constants.h.
     * @return {number} The index within |this.marginList_| that corrsponds to
     *     |marginTypeValue|.
     * @private
     */
    getMarginOptionIndexByValue_: function(marginTypeValue) {
      var options = this.marginList_.options;
      for (var i = 0; i < options.length; i++) {
        if (options[i].getAttribute('value') == marginTypeValue)
          return i;
      }
      return MarginSettings.OPTION_INDEX_DEFAULT;
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
     * @return {boolean} True if minimum margins are selected.
     */
    isMinimumMarginsSelected: function() {
      return this.selectedMarginsValue == MarginSettings.MARGINS_VALUE_MINIMUM;
    },

    /**
     * If the custom margin values have changed then request a new preview based
     * on the newly set margins.
     * @private
     */
    requestPreviewIfNeeded_: function() {
      if (!this.areMarginSettingsValid())
        return;

      if (this.customMargins_.isEqual(this.previousCustomMargins_))
        return;

      this.previousCustomMargins_ = this.customMargins_.clone();
      setDefaultValuesAndRegeneratePreview(false);
    },

    /**
     * Listener executed when the mouse is over the sidebar. If the custom
     * margin lines are displayed, then, it fades them out.
     * @private
     */
    onSidebarMouseOver_: function(e) {
      $('mainview').onmouseover = this.onMainviewMouseOver_.bind(this);
      $('navbar-container').onmouseover = null;
      if (!this.forceDisplayingMarginLines_)
        this.marginsUI.hide(false);
    },

    /**
     * Listener executed when the mouse is over the main view. If the custom
     * margin lines are hidden, then, it fades them in.
     * @private
     */
    onMainviewMouseOver_: function() {
      $('mainview').onmouseover = null;
      $('navbar-container').onmouseover = this.onSidebarMouseOver_.bind(this);
      this.forceDisplayingMarginLines_ = false;
      this.marginsUI.show();
    },

    /**
     * Adds listeners to all margin related controls.
     * @private
     */
    addEventListeners_: function() {
      this.marginList_.onchange = this.onMarginsChanged_.bind(this);
      document.addEventListener(customEvents.PDF_LOADED,
                                this.onPDFLoaded_.bind(this));
      document.addEventListener(customEvents.PDF_GENERATION_ERROR,
                                this.onPDFGenerationError_.bind(this));
    },

    /**
     * Executes when a |customEvents.PDF_GENERATION_ERROR| event occurs.
     * @private
     */
    onPDFGenerationError_: function() {
      if (this.isCustomMarginsSelected()) {
        this.removeCustomMarginEventListeners_();
        this.marginsUI.hide(true);
      }
    },

    /**
     * Executes whenever a |customEvents.MARGIN_LINE_DRAG| occurs.
     * @param {cr.Event} e The event that triggered this listener.
     */
    onMarginsLineDrag_: function(e) {
      var dragDeltaInPoints = this.convertDragDeltaToPoints_(e.dragDelta);
      this.marginsUI.lastClickedMarginsUIPair.updateWhileDragging(
          dragDeltaInPoints, e.destinationPoint);
    },

    /**
     * @param {number} dragDelta The difference in pixels between the original
     *     and current postion of the last clicked margin line.
     * @return {number} The difference in points.
     * @private
     */
    convertDragDeltaToPoints_: function(dragDelta) {
      if (this.marginsUI.lastClickedMarginsUIPair.isTop_() ||
          this.marginsUI.lastClickedMarginsUIPair.isBottom_()) {
        return dragDelta * this.totalHeightInPoints;
      } else {
        return dragDelta * this.totalWidthInPoints;
      }
    },

    /**
     * @return {boolean} True if the margin settings are valid.
     */
    areMarginSettingsValid: function() {
      if (!this.isCustomMarginsSelected() || !this.marginsUI_)
        return true;

      var pairs = this.marginsUI.pairsAsList;
      return pairs.every(function(pair) { return pair.box_.isValid; });
    },

    /**
     * Calculates the maximum allowable value of the selected margin text for
     * every margin.
     * @return {array} The maximum allowable value in points in order top, left,
     *     right, bottom.
     * @private
     */
    getMarginValueLimits_: function() {
      var marginValueLimits = [];
      marginValueLimits[0] = this.pageHeight_ - this.customMargins_.bottom -
          MarginSettings.MINIMUM_MARGINS_DISTANCE;
      marginValueLimits[1] = this.pageWidth_ - this.customMargins_.right -
          MarginSettings.MINIMUM_MARGINS_DISTANCE;
      marginValueLimits[2] = this.pageWidth_ - this.customMargins_.left -
          MarginSettings.MINIMUM_MARGINS_DISTANCE;
      marginValueLimits[3] = this.pageHeight_ - this.customMargins_.top -
          MarginSettings.MINIMUM_MARGINS_DISTANCE;

      for (var i = 0; i < marginValueLimits.length; i++) {
        marginValueLimits[i] = Math.max(marginValueLimits[i], 0);
        marginValueLimits[i] = print_preview.convertPointsToLocaleUnitsAndBack(
            marginValueLimits[i]);
      }
      return marginValueLimits;
    },

    /**
     * @return {array} The margin value limits positions normalized to the total
     *     width and height of the plugin and with respect to the top left
     *     corner of the plugin.
     */
    getMarginValueLimitsInPercent_: function() {
      var pageInformation = previewArea.pageLocationNormalized;
      var totalWidthInPoints = this.pageWidth_ / pageInformation.width;
      var totalHeightInPoints = this.pageHeight_ / pageInformation.height;
      var marginValueLimits = this.getMarginValueLimits_();
      var marginValueLimitsInPercent = [];
      marginValueLimitsInPercent[0] = pageInformation.y + marginValueLimits[0] /
          totalHeightInPoints;
      marginValueLimitsInPercent[1] = pageInformation.x + marginValueLimits[1] /
          totalWidthInPoints;
      marginValueLimitsInPercent[2] = pageInformation.x +
          pageInformation.width - marginValueLimits[2] / totalWidthInPoints;
      marginValueLimitsInPercent[3] = pageInformation.y +
          pageInformation.height - marginValueLimits[3] / totalHeightInPoints;
      return marginValueLimitsInPercent;
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
      var marginBoxValue =
          print_preview.convertLocaleUnitsToPoints(marginBox.margin);
      this.customMargins_[marginBox.marginGroup] = marginBoxValue;
      this.requestPreviewIfNeeded_();
    },

    /**
     * @type {print_preview.MarginsUI} The object holding the UI for specifying
     *     custom margins.
     */
    get marginsUI() {
      if (!this.marginsUI_) {
        this.marginsUI_ = new print_preview.MarginsUI();
        $('mainview').appendChild(this.marginsUI_);
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
      $('navbar-container').onmouseover = this.onSidebarMouseOver_.bind(this);
      this.eventTracker_.add(this.marginsUI,
                             customEvents.MARGIN_LINE_DRAG,
                             this.onMarginsLineDrag_.bind(this),
                             false);
      this.eventTracker_.add(document, customEvents.MARGIN_TEXTBOX_FOCUSED,
                             this.onMarginTextboxFocused_.bind(this), false);
    },

    /**
     * Removes the event listeners associated with the custom margins option.
     * @private
     */
    removeCustomMarginEventListeners_: function() {
      if (!this.marginsUI_)
        return;
      $('mainview').onmouseover = null;
      $('navbar-container').onmouseover = null;
      this.eventTracker_.remove(this.marginsUI, customEvents.MARGIN_LINE_DRAG);
      this.eventTracker_.remove(document, customEvents.MARGIN_TEXTBOX_FOCUSED);
      this.marginsUI.hide(true);
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
      previewArea.update();
      var keepDisplayedValue = !this.areMarginSettingsValid();
      this.marginsUI.update(this.getMarginsRectangleInPercent_(),
                            this.customMargins_,
                            this.getMarginValueLimits_(),
                            keepDisplayedValue,
                            this.getMarginValueLimitsInPercent_());
      this.marginsUI.draw();
    },

    /**
     * Called when there is change in the preview position or size.
     */
    onPreviewPositionChanged: function() {
      if (!previewArea.pdfPlugin)
        return;
      if (this.isCustomMarginsSelected() && previewArea.pdfLoaded &&
          pageSettings.totalPageCount != undefined) {
        this.updatePageData_();
        this.drawCustomMarginsUI_();
      }
    },

    /**
     * Executes when a margin textbox is focused. Used for improved
     * accessibility.
     * @private
     */
    onMarginTextboxFocused_: function() {
      this.forceDisplayingMarginLines_ = true;
      this.marginsUI.show();
    },

    /**
     * Executes when user selects a different margin option, ie,
     * |this.marginList_.selectedIndex| is changed.
     * @private
     */
    onMarginsChanged_: function() {
      if (this.isDefaultMarginsSelected() || this.isMinimumMarginsSelected() ||
          this.isNoMarginsSelected())
        this.onDefaultMinimumNoMarginsSelected_();
      else if (this.isCustomMarginsSelected())
        this.onCustomMarginsSelected_();
    },

    /**
     * Executes when the default or minimum or no margins option is selected.
     * @private
     */
    onDefaultMinimumNoMarginsSelected_: function() {
      this.removeCustomMarginEventListeners_();
      this.forceDisplayingMarginLines_ = true;
      this.previousCustomMargins_ = null;
      setDefaultValuesAndRegeneratePreview(false);
    },

    /**
     * Executes when the custom margins option is selected.
     * @private
     */
    onCustomMarginsSelected_: function() {
      if (!previewArea.pdfPlugin) {
        this.forceMarginsUIOnPDFLoad_ = true;
        return;
      }
      var customMarginsNotSpecified = !this.customMargins_;
      this.updatePageData_();

      if (customMarginsNotSpecified) {
        this.previousCustomMargins_ = this.customMargins_.clone();
        this.drawCustomMarginsUI_();
        this.addCustomMarginEventListeners_();
        this.marginsUI.show();
      } else {
        this.forceMarginsUIOnPDFLoad_ = true;
        this.requestPreviewIfNeeded_();
      }
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
      var pageLocation = previewArea.pageLocationNormalized;
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
      return this.convertMarginsInPointsToPercent(this.customMargins_);
    },

    /**
     * Converts |marginsToConvert| to points and normalizes it to the height and
     * width of the plugin.
     * @return {print_preview.Margins} The margins in percent.
     * @private
     */
    convertMarginsInPointsToPercent: function(marginsToConvert) {
      var pageInformation = previewArea.pageLocationNormalized;
      var totalWidthInPoints = this.pageWidth_ / pageInformation.width;
      var totalHeightInPoints = this.pageHeight_ / pageInformation.height;
      var marginsInPercent = new Margins(
          marginsToConvert.left / totalWidthInPoints,
          marginsToConvert.top / totalHeightInPoints,
          marginsToConvert.right / totalWidthInPoints,
          marginsToConvert.bottom / totalHeightInPoints);
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
            MarginSettings.OPTION_INDEX_DEFAULT].selected = true;
        this.removeCustomMarginEventListeners_();
        this.forceDisplayingMarginLines_ = true;
        this.customMargins_ = null;
        this.previousCustomMargins_ = null;
      }
    },

    /**
     * Executes when a |customEvents.PDF_LOADED| event occurs.
     * @private
     */
    onPDFLoaded_: function() {
      if (!previewModifiable) {
        fadeOutOption(this.marginsOption_);
        return;
      }

      if (this.forceMarginsUIOnPDFLoad_) {
        this.updatePageData_();
        this.drawCustomMarginsUI_();
        this.addCustomMarginEventListeners_();
        this.marginsUI.show();
        this.forceMarginsUIOnPDFLoad_ = false;
      }
    },

    /**
     * Updates |this.customMargins_|, |this.pageWidth_|, |this.pageHeight_|.
     * @private
     */
    updatePageData_: function() {
      if (!this.customMargins_)
        this.customMargins_ = this.currentDefaultPageLayout.margins_.clone();

      this.pageWidth_ = this.currentDefaultPageLayout.pageWidth;
      this.pageHeight_ = this.currentDefaultPageLayout.pageHeight;
    }
  };

  return {
    MarginSettings: MarginSettings,
    PageLayout: PageLayout,
  };
});
