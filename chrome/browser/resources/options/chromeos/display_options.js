// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('options');

/**
 * Enumeration of display layout. These values must match the C++ values in
 * ash::DisplayController.
 * @enum {number}
 */
options.DisplayLayoutType = {
  TOP: 0,
  RIGHT: 1,
  BOTTOM: 2,
  LEFT: 3
};

/**
 * Enumeration of multi display mode. These values must match the C++ values in
 * ash::DisplayManager.
 * @enum {number}
 */
options.MultiDisplayMode = {
  EXTENDED: 0,
  MIRRORING: 1,
  UNIFIED: 2,
};

/**
 * @typedef {{
 *   left: number,
 *   top: number,
 *   width: number,
 *   height: number
 * }}
 */
options.DisplayBounds;

/**
 * @typedef {{
 *   x: number,
 *   y: number
 * }}
 */
options.DisplayPosition;

/**
 * @typedef {{
 *   width: number,
 *   height: number,
 *   originalWidth: number,
 *   originalHeight: number,
 *   deviceScaleFactor: number,
 *   scale: number,
 *   refreshRate: number,
 *   isBest: boolean,
 *   selected: boolean
 * }}
 */
options.DisplayMode;

/**
 * @typedef {{
 *   profileId: number,
 *   name: string
 * }}
 */
options.ColorProfile;

/**
 * @typedef {{
 *   availableColorProfiles: !Array<!options.ColorProfile>,
 *   bounds: !options.DisplayBounds,
 *   colorProfileId: number,
 *   id: string,
 *   isInternal: boolean,
 *   isPrimary: boolean,
 *   resolutions: !Array<!options.DisplayMode>,
 *   name: string,
 *   rotation: number
 * }}
 */
options.DisplayInfo;

/**
 * @typedef {{
 *   bounds: !options.DisplayBounds,
 *   div: ?HTMLElement,
 *   id: string,
 *   isPrimary: boolean,
 *   layoutType: options.DisplayLayoutType,
 *   name: string,
 *   originalPosition: !options.DisplayPosition
 * }}
 */
options.DisplayLayout;

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  // The scale ratio of the display rectangle to its original size.
  /** @const */ var VISUAL_SCALE = 1 / 10;

  // The number of pixels to share the edges between displays.
  /** @const */ var MIN_OFFSET_OVERLAP = 5;

  /**
   * Gets the layout type of |point| relative to |rect|.
   * @param {!options.DisplayBounds} rect The base rectangle.
   * @param {!options.DisplayPosition} point The point to check the position.
   * @return {options.DisplayLayoutType} The position of the calculated point.
   */
  function getPositionToRectangle(rect, point) {
    // Separates the area into four (LEFT/RIGHT/TOP/BOTTOM) by the diagonals of
    // the rect, and decides which area the display should reside.
    var diagonalSlope = rect.height / rect.width;
    var topDownIntercept = rect.top - rect.left * diagonalSlope;
    var bottomUpIntercept = rect.top + rect.height + rect.left * diagonalSlope;

    if (point.y > topDownIntercept + point.x * diagonalSlope) {
      if (point.y > bottomUpIntercept - point.x * diagonalSlope)
        return options.DisplayLayoutType.BOTTOM;
      else
        return options.DisplayLayoutType.LEFT;
    } else {
      if (point.y > bottomUpIntercept - point.x * diagonalSlope)
        return options.DisplayLayoutType.RIGHT;
      else
        return options.DisplayLayoutType.TOP;
    }
  }

  /**
   * Snaps the region [point, width] to [basePoint, baseWidth] if
   * the [point, width] is close enough to the base's edge.
   * @param {number} point The starting point of the region.
   * @param {number} width The width of the region.
   * @param {number} basePoint The starting point of the base region.
   * @param {number} baseWidth The width of the base region.
   * @return {number} The moved point. Returns the point itself if it doesn't
   *     need to snap to the edge.
   * @private
   */
  function snapToEdge(point, width, basePoint, baseWidth) {
    // If the edge of the region is smaller than this, it will snap to the
    // base's edge.
    /** @const */ var SNAP_DISTANCE_PX = 16;

    var startDiff = Math.abs(point - basePoint);
    var endDiff = Math.abs(point + width - (basePoint + baseWidth));
    // Prefer the closer one if both edges are close enough.
    if (startDiff < SNAP_DISTANCE_PX && startDiff < endDiff)
      return basePoint;
    else if (endDiff < SNAP_DISTANCE_PX)
      return basePoint + baseWidth - width;

    return point;
  }

  /**
   * Encapsulated handling of the 'Display' page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function DisplayOptions() {
    Page.call(this, 'display',
              loadTimeData.getString('displayOptionsPageTabTitle'),
              'display-options-page');
  }

  cr.addSingletonGetter(DisplayOptions);

  DisplayOptions.prototype = {
    __proto__: Page.prototype,

    /**
     * Whether the current output status is mirroring displays or not.
     * @type {boolean}
     * @private
     */
    mirroring_: false,

    /**
     * Whether the unified desktop is enable or not.
     * @type {boolean}
     * @private
     */
    unifiedDesktopEnabled_: false,

    /**
     * Whether the unified desktop option should be present.
     * @type {boolean}
     * @private
     */
    showUnifiedDesktopOption_: false,

    /**
     * The array of current output displays.  It also contains the display
     * rectangles currently rendered on screen.
     * @type {!Array<!options.DisplayInfo>}
     * @private
     */
    displays_: [],

    /**
     * An object containing DisplayLayout objects for each entry in |displays_|.
     * @type {!Object<!options.DisplayLayout>}
     * @private
     */
    displayLayoutMap_: {},

    /**
     * The id of the currently focused display, or empty for none.
     * @type {string}
     * @private
     */
    focusedId_: '',

    /**
     * The primary display id.
     * @type {string}
     * @private
     */
    primaryDisplayId_: '',

    /**
     * The secondary display id.
     * @type {string}
     * @private
     */
    secondaryDisplayId_: '',

    /**
     * Drag info.
     * @type {?{displayId: string,
     *          originalLocation: !options.DisplayPosition,
     *          eventLocation: !options.DisplayPosition}}
     * @private
     */
    dragInfo_: null,

    /**
     * The container div element which contains all of the display rectangles.
     * @type {?Element}
     * @private
     */
    displaysView_: null,

    /**
     * The scale factor of the actual display size to the drawn display
     * rectangle size.
     * @type {number}
     * @private
     */
    visualScale_: VISUAL_SCALE,

    /**
     * The location where the last touch event happened.  This is used to
     * prevent unnecessary dragging events happen.  Set to null unless it's
     * during touch events.
     * @type {?options.DisplayPosition}
     * @private
     */
    lastTouchLocation_: null,

    /**
     * Whether the display settings can be shown.
     * @type {boolean}
     * @private
     */
    enabled_: true,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('display-options-select-mirroring').onchange = (function() {
        this.mirroring_ =
            $('display-options-select-mirroring').value == 'mirroring';
        chrome.send('setMirroring', [this.mirroring_]);
      }).bind(this);

      var container = $('display-options-displays-view-host');
      container.onmousemove = this.onMouseMove_.bind(this);
      window.addEventListener('mouseup', this.endDragging_.bind(this), true);
      container.ontouchmove = this.onTouchMove_.bind(this);
      container.ontouchend = this.endDragging_.bind(this);

      $('display-options-set-primary').onclick = (function() {
        chrome.send('setPrimary', [this.focusedId_]);
      }).bind(this);
      $('display-options-resolution-selection').onchange = (function(ev) {
        var display = this.getDisplayInfoFromId(this.focusedId_);
        var resolution = display.resolutions[ev.target.value];
        chrome.send('setDisplayMode', [this.focusedId_, resolution]);
      }).bind(this);
      $('display-options-orientation-selection').onchange = (function(ev) {
        var rotation = parseInt(ev.target.value, 10);
        chrome.send('setRotation', [this.focusedId_, rotation]);
      }).bind(this);
      $('display-options-color-profile-selection').onchange = (function(ev) {
        chrome.send('setColorProfile', [this.focusedId_, ev.target.value]);
      }).bind(this);
      $('selected-display-start-calibrating-overscan').onclick = (function() {
        // Passes the target display ID. Do not specify it through URL hash,
        // we do not care back/forward.
        var displayOverscan = options.DisplayOverscan.getInstance();
        displayOverscan.setDisplayId(this.focusedId_);
        PageManager.showPageByName('displayOverscan');
        chrome.send('coreOptionsUserMetricsAction',
                    ['Options_DisplaySetOverscan']);
      }).bind(this);

      $('display-options-done').onclick = function() {
        PageManager.closeOverlay();
      };

      $('display-options-toggle-unified-desktop').onclick = (function() {
        this.unifiedDesktopEnabled_ = !this.unifiedDesktopEnabled_;
        chrome.send('setUnifiedDesktopEnabled',
                    [this.unifiedDesktopEnabled_]);
      }).bind(this);
    },

    /** @override */
    didShowPage: function() {
      var optionTitles = document.getElementsByClassName(
          'selected-display-option-title');
      var maxSize = 0;
      for (var i = 0; i < optionTitles.length; i++)
        maxSize = Math.max(maxSize, optionTitles[i].clientWidth);
      for (var i = 0; i < optionTitles.length; i++)
        optionTitles[i].style.width = maxSize + 'px';
      chrome.send('getDisplayInfo');
    },

    /** @override */
    canShowPage: function() { return this.enabled_; },

    /**
     * Enables or disables the page. When disabled, the page will not be able to
     * open, and will close if currently opened.
     * @param {boolean} enabled Whether the page should be enabled.
     * @param {boolean} showUnifiedDesktop Whether the unified desktop option
     *     should be present.
     */
    setEnabled: function(enabled, showUnifiedDesktop) {
      if (this.enabled_ == enabled &&
          this.showUnifiedDesktopOption_ == showUnifiedDesktop) {
        return;
      }
      this.enabled_ = enabled;
      this.showUnifiedDesktopOption_ = showUnifiedDesktop;
      if (!enabled && this.visible)
        PageManager.closeOverlay();
    },

    /**
     * Mouse move handler for dragging display rectangle.
     * @param {Event} e The mouse move event.
     * @private
     */
    onMouseMove_: function(e) {
      return this.processDragging_(e, {x: e.pageX, y: e.pageY});
    },

    /**
     * Touch move handler for dragging display rectangle.
     * @param {Event} e The touch move event.
     * @private
     */
    onTouchMove_: function(e) {
      if (e.touches.length != 1)
        return true;

      var touchLocation = {x: e.touches[0].pageX, y: e.touches[0].pageY};
      // Touch move events happen even if the touch location doesn't change, but
      // it doesn't need to process the dragging.  Since sometimes the touch
      // position changes slightly even though the user doesn't think to move
      // the finger, very small move is just ignored.
      /** @const */ var IGNORABLE_TOUCH_MOVE_PX = 1;
      var xDiff = Math.abs(touchLocation.x - this.lastTouchLocation_.x);
      var yDiff = Math.abs(touchLocation.y - this.lastTouchLocation_.y);
      if (xDiff <= IGNORABLE_TOUCH_MOVE_PX &&
          yDiff <= IGNORABLE_TOUCH_MOVE_PX) {
        return true;
      }

      this.lastTouchLocation_ = touchLocation;
      return this.processDragging_(e, touchLocation);
    },

    /**
     * Mouse down handler for dragging display rectangle.
     * @param {Event} e The mouse down event.
     * @private
     */
    onMouseDown_: function(e) {
      if (this.mirroring_)
        return true;

      if (e.button != 0)
        return true;

      e.preventDefault();
      var target = assertInstanceof(e.target, HTMLElement);
      return this.startDragging_(target, {x: e.pageX, y: e.pageY});
    },

    /**
     * Touch start handler for dragging display rectangle.
     * @param {Event} e The touch start event.
     * @private
     */
    onTouchStart_: function(e) {
      if (this.mirroring_)
        return true;

      if (e.touches.length != 1)
        return false;

      e.preventDefault();
      var touch = e.touches[0];
      this.lastTouchLocation_ = {x: touch.pageX, y: touch.pageY};
      var target = assertInstanceof(e.target, HTMLElement);
      return this.startDragging_(target, this.lastTouchLocation_);
    },

    /**
     * @param {string} id
     * @return {options.DisplayInfo}
     */
    getDisplayInfoFromId(id) {
      return this.displays_.find(function(display) {
        return display.id == id;
      });
    },

    /**
     * Collects the current data and sends it to Chrome.
     * @private
     */
    sendDragResult_: function() {
      // Offset is calculated from top or left edge.
      var primary = this.displayLayoutMap_[this.primaryDisplayId_];
      var secondary = this.displayLayoutMap_[this.secondaryDisplayId_];
      var layoutType = secondary.layoutType;
      var offset;
      if (layoutType == options.DisplayLayoutType.LEFT ||
          layoutType == options.DisplayLayoutType.RIGHT) {
        offset = secondary.div.offsetTop - primary.div.offsetTop;
      } else {
        offset = secondary.div.offsetLeft - primary.div.offsetLeft;
      }
      offset = Math.floor(offset / this.visualScale_);
      chrome.send('setDisplayLayout', [secondary.id, layoutType, offset]);
    },

    /**
     * Processes the actual dragging of display rectangle.
     * @param {Event} e The event which triggers this drag.
     * @param {options.DisplayPosition} eventLocation The location where the
     *     event happens.
     * @private
     */
    processDragging_: function(e, eventLocation) {
      if (!this.dragInfo_)
        return true;

      e.preventDefault();

      // Note that current code of moving display-rectangles doesn't work
      // if there are >=3 displays.  This is our assumption for M21.
      // TODO(mukai): Fix the code to allow >=3 displays.
      var dragInfo = this.dragInfo_;
      /** @type {options.DisplayPosition} */ var newPosition = {
        x: dragInfo.originalLocation.x +
            (eventLocation.x - dragInfo.eventLocation.x),
        y: dragInfo.originalLocation.y +
            (eventLocation.y - dragInfo.eventLocation.y)
      };

      var dragLayout = this.displayLayoutMap_[dragInfo.displayId];
      var draggingDiv = dragLayout.div;

      var baseDisplayId = dragLayout.isPrimary ? this.secondaryDisplayId_ :
                                                 this.primaryDisplayId_;
      var baseLayout = this.displayLayoutMap_[baseDisplayId];
      var baseDiv = baseLayout.div;

      newPosition.x = snapToEdge(
          newPosition.x, draggingDiv.offsetWidth, baseDiv.offsetLeft,
          baseDiv.offsetWidth);
      newPosition.y = snapToEdge(
          newPosition.y, draggingDiv.offsetHeight, baseDiv.offsetTop,
          baseDiv.offsetHeight);

      /** @type {!options.DisplayPosition} */ var newCenter = {
        x: newPosition.x + draggingDiv.offsetWidth / 2,
        y: newPosition.y + draggingDiv.offsetHeight / 2
      };

      /** @type {!options.DisplayBounds} */ var baseBounds = {
        left: baseDiv.offsetLeft,
        top: baseDiv.offsetTop,
        width: baseDiv.offsetWidth,
        height: baseDiv.offsetHeight
      };

      var isPrimary = dragLayout.isPrimary;
      // layoutType is always stored in the secondary layout.
      var layoutType =
          isPrimary ? baseLayout.layoutType : dragLayout.layoutType;

      switch (getPositionToRectangle(baseBounds, newCenter)) {
        case options.DisplayLayoutType.RIGHT:
          layoutType = isPrimary ? options.DisplayLayoutType.LEFT :
                                   options.DisplayLayoutType.RIGHT;
          break;
        case options.DisplayLayoutType.LEFT:
          layoutType = isPrimary ? options.DisplayLayoutType.RIGHT :
                                   options.DisplayLayoutType.LEFT;
          break;
        case options.DisplayLayoutType.TOP:
          layoutType = isPrimary ? options.DisplayLayoutType.BOTTOM :
                                   options.DisplayLayoutType.TOP;
          break;
        case options.DisplayLayoutType.BOTTOM:
          layoutType = isPrimary ? options.DisplayLayoutType.TOP :
                                   options.DisplayLayoutType.BOTTOM;
          break;
      }

      if (layoutType == options.DisplayLayoutType.LEFT ||
          layoutType == options.DisplayLayoutType.RIGHT) {
        if (newPosition.y > baseDiv.offsetTop + baseDiv.offsetHeight)
          layoutType = isPrimary ? options.DisplayLayoutType.TOP :
                                   options.DisplayLayoutType.BOTTOM;
        else if (newPosition.y + draggingDiv.offsetHeight < baseDiv.offsetTop)
          layoutType = isPrimary ? options.DisplayLayoutType.BOTTOM :
                                   options.DisplayLayoutType.TOP;
      } else {
        if (newPosition.x > baseDiv.offsetLeft + baseDiv.offsetWidth)
          layoutType = isPrimary ? options.DisplayLayoutType.LEFT :
                                   options.DisplayLayoutType.RIGHT;
        else if (newPosition.x + draggingDiv.offsetWidth < baseDiv.offsetLeft)
          layoutType = isPrimary ? options.DisplayLayoutType.RIGHT :
                                   options.DisplayLayoutType.LEFT;
      }

      var layoutToBase;
      if (!isPrimary) {
        dragLayout.layoutType = layoutType;
        layoutToBase = layoutType;
      } else {
        baseLayout.layoutType = layoutType;
        switch (layoutType) {
          case options.DisplayLayoutType.RIGHT:
            layoutToBase = options.DisplayLayoutType.LEFT;
            break;
          case options.DisplayLayoutType.LEFT:
            layoutToBase = options.DisplayLayoutType.RIGHT;
            break;
          case options.DisplayLayoutType.TOP:
            layoutToBase = options.DisplayLayoutType.BOTTOM;
            break;
          case options.DisplayLayoutType.BOTTOM:
            layoutToBase = options.DisplayLayoutType.TOP;
            break;
        }
      }

      switch (layoutToBase) {
        case options.DisplayLayoutType.RIGHT:
          draggingDiv.style.left =
              baseDiv.offsetLeft + baseDiv.offsetWidth + 'px';
          draggingDiv.style.top = newPosition.y + 'px';
          break;
        case options.DisplayLayoutType.LEFT:
          draggingDiv.style.left =
              baseDiv.offsetLeft - draggingDiv.offsetWidth + 'px';
          draggingDiv.style.top = newPosition.y + 'px';
          break;
        case options.DisplayLayoutType.TOP:
          draggingDiv.style.top =
              baseDiv.offsetTop - draggingDiv.offsetHeight + 'px';
          draggingDiv.style.left = newPosition.x + 'px';
          break;
        case options.DisplayLayoutType.BOTTOM:
          draggingDiv.style.top =
              baseDiv.offsetTop + baseDiv.offsetHeight + 'px';
          draggingDiv.style.left = newPosition.x + 'px';
          break;
      }

      return false;
    },

    /**
     * Start dragging of a display rectangle.
     * @param {!HTMLElement} target The event target.
     * @param {!options.DisplayPosition} eventLocation The event location.
     * @private
     */
    startDragging_: function(target, eventLocation) {
      var oldFocusedId = this.focusedId_;
      var newFocusedId;
      var willUpdateDisplayDescription = false;
      for (var i = 0; i < this.displays_.length; i++) {
        var displayLayout = this.displayLayoutMap_[this.displays_[i].id];
        if (displayLayout.div == target ||
            (target.offsetParent && target.offsetParent == displayLayout.div)) {
          newFocusedId = displayLayout.id;
          break;
        }
      }
      if (!newFocusedId)
        return false;

      this.focusedId_ = newFocusedId;
      willUpdateDisplayDescription = newFocusedId != oldFocusedId;

      for (var i = 0; i < this.displays_.length; i++) {
        var displayLayout = this.displayLayoutMap_[this.displays_[i].id];
        displayLayout.div.className = 'displays-display';
        if (displayLayout.id != this.focusedId_)
          continue;

        displayLayout.div.classList.add('displays-focused');
        if (this.displays_.length > 1) {
          this.dragInfo_ = {
            displayId: displayLayout.id,
            originalLocation: {
              x: displayLayout.div.offsetLeft,
              y: displayLayout.div.offsetTop
            },
            eventLocation: {x: eventLocation.x, y: eventLocation.y}
          };
        }
      }

      if (willUpdateDisplayDescription)
        this.updateSelectedDisplayDescription_();
      return false;
    },

    /**
     * finish the current dragging of displays.
     * @param {Event} e The event which triggers this.
     * @private
     */
    endDragging_: function(e) {
      this.lastTouchLocation_ = null;
      if (!this.dragInfo_)
        return false;

      // Make sure the dragging location is connected.
      var dragLayout = this.displayLayoutMap_[this.dragInfo_.displayId];
      var baseDisplayId = dragLayout.isPrimary ? this.secondaryDisplayId_ :
                                                 this.primaryDisplayId_;

      var baseLayout = this.displayLayoutMap_[baseDisplayId];
      var baseDiv = baseLayout.div;
      var draggingDiv = dragLayout.div;

      // layoutType is always stored in the secondary layout.
      var layoutType =
          dragLayout.isPrimary ? baseLayout.layoutType : dragLayout.layoutType;

      if (layoutType == options.DisplayLayoutType.LEFT ||
          layoutType == options.DisplayLayoutType.RIGHT) {
        var top = Math.max(
            draggingDiv.offsetTop,
            baseDiv.offsetTop - draggingDiv.offsetHeight + MIN_OFFSET_OVERLAP);
        top = Math.min(
            top, baseDiv.offsetTop + baseDiv.offsetHeight - MIN_OFFSET_OVERLAP);
        draggingDiv.style.top = top + 'px';
      } else {
        var left = Math.max(
            draggingDiv.offsetLeft,
            baseDiv.offsetLeft - draggingDiv.offsetWidth + MIN_OFFSET_OVERLAP);
        left = Math.min(
            left,
            baseDiv.offsetLeft + baseDiv.offsetWidth - MIN_OFFSET_OVERLAP);
        draggingDiv.style.left = left + 'px';
      }
      if (dragLayout.originalPosition.x != draggingDiv.offsetLeft ||
          dragLayout.originalPosition.y != draggingDiv.offsetTop) {
        this.sendDragResult_();
      }

      this.dragInfo_ = null;

      return false;
    },

    /**
     * Updates the description of selected display section for mirroring mode.
     * @private
     */
    updateSelectedDisplaySectionMirroring_: function() {
      $('display-configuration-arrow').hidden = true;
      $('display-options-set-primary').disabled = true;
      $('display-options-select-mirroring').disabled = false;
      $('selected-display-start-calibrating-overscan').disabled = true;
      var display = this.displays_[0];
      var orientation = $('display-options-orientation-selection');
      orientation.disabled = false;
      var orientationOptions = orientation.getElementsByTagName('option');
      var orientationIndex = Math.floor(display.rotation / 90);
      orientationOptions[orientationIndex].selected = true;
      $('selected-display-name').textContent =
          loadTimeData.getString('mirroringDisplay');
      var resolution = $('display-options-resolution-selection');
      var option = document.createElement('option');
      option.value = 'default';
      option.textContent = display.bounds.width + 'x' + display.bounds.height;
      resolution.appendChild(option);
      resolution.disabled = true;
    },

    /**
     * Updates the description of selected display section when no display is
     * selected.
     * @private
     */
    updateSelectedDisplaySectionNoSelected_: function() {
      $('display-configuration-arrow').hidden = true;
      $('display-options-set-primary').disabled = true;
      $('display-options-select-mirroring').disabled = true;
      $('selected-display-start-calibrating-overscan').disabled = true;
      $('display-options-orientation-selection').disabled = true;
      $('selected-display-name').textContent = '';
      var resolution = $('display-options-resolution-selection');
      resolution.appendChild(document.createElement('option'));
      resolution.disabled = true;
    },

    /**
     * Updates the description of selected display section for the selected
     * display.
     * @param {options.DisplayInfo} display The selected display object.
     * @private
     */
    updateSelectedDisplaySectionForDisplay_: function(display) {
      var displayLayout = this.displayLayoutMap_[display.id];
      var arrow = $('display-configuration-arrow');
      arrow.hidden = false;
      // Adding 1 px to the position to fit the border line and the border in
      // arrow precisely.
      arrow.style.top = $('display-configurations').offsetTop -
          arrow.offsetHeight / 2 + 'px';
      arrow.style.left = displayLayout.div.offsetLeft +
          displayLayout.div.offsetWidth / 2 - arrow.offsetWidth / 2 + 'px';

      $('display-options-set-primary').disabled = display.isPrimary;
      $('display-options-select-mirroring').disabled =
          (this.displays_.length <= 1 && !this.unifiedDesktopEnabled_);
      $('selected-display-start-calibrating-overscan').disabled =
          display.isInternal;

      var orientation = $('display-options-orientation-selection');
      orientation.disabled = this.unifiedDesktopEnabled_;

      var orientationOptions = orientation.getElementsByTagName('option');
      var orientationIndex = Math.floor(display.rotation / 90);
      orientationOptions[orientationIndex].selected = true;

      $('selected-display-name').textContent = display.name;

      var resolution = $('display-options-resolution-selection');
      if (display.resolutions.length <= 1) {
        var option = document.createElement('option');
        option.value = 'default';
        option.textContent = display.bounds.width + 'x' + display.bounds.height;
        option.selected = true;
        resolution.appendChild(option);
        resolution.disabled = true;
      } else {
        var previousOption;
        for (var i = 0; i < display.resolutions.length; i++) {
          var option = document.createElement('option');
          option.value = i;
          option.textContent = display.resolutions[i].width + 'x' +
              display.resolutions[i].height;
          if (display.resolutions[i].isBest) {
            option.textContent += ' ' +
                loadTimeData.getString('annotateBest');
          } else if (display.resolutions[i].isNative) {
            option.textContent += ' ' +
                loadTimeData.getString('annotateNative');
          }
          if (display.resolutions[i].deviceScaleFactor && previousOption &&
              previousOption.textContent == option.textContent) {
            option.textContent +=
                ' (' + display.resolutions[i].deviceScaleFactor + 'x)';
          }
          option.selected = display.resolutions[i].selected;
          resolution.appendChild(option);
          previousOption = option;
        }
        resolution.disabled = (display.resolutions.length <= 1);
      }

      if (display.availableColorProfiles.length <= 1) {
        $('selected-display-color-profile-row').hidden = true;
      } else {
        $('selected-display-color-profile-row').hidden = false;
        var profiles = $('display-options-color-profile-selection');
        profiles.innerHTML = '';
        for (var i = 0; i < display.availableColorProfiles.length; i++) {
          var option = document.createElement('option');
          var colorProfile = display.availableColorProfiles[i];
          option.value = colorProfile.profileId;
          option.textContent = colorProfile.name;
          option.selected = (display.colorProfileId == colorProfile.profileId);
          profiles.appendChild(option);
        }
      }
    },

    /**
     * Updates the description of the selected display section.
     * @private
     */
    updateSelectedDisplayDescription_: function() {
      var resolution = $('display-options-resolution-selection');
      resolution.textContent = '';
      var orientation = $('display-options-orientation-selection');
      var orientationOptions = orientation.getElementsByTagName('option');
      for (var i = 0; i < orientationOptions.length; i++)
        orientationOptions[i].selected = false;

      if (this.mirroring_) {
        this.updateSelectedDisplaySectionMirroring_();
      } else if (this.focusedId_ == '') {
        this.updateSelectedDisplaySectionNoSelected_();
      } else {
        this.updateSelectedDisplaySectionForDisplay_(
            this.getDisplayInfoFromId(this.focusedId_));
      }
    },

    /**
     * Clears the drawing area for display rectangles.
     * @private
     */
    resetDisplaysView_: function() {
      var displaysViewHost = $('display-options-displays-view-host');
      displaysViewHost.removeChild(displaysViewHost.firstChild);
      this.displaysView_ = document.createElement('div');
      this.displaysView_.id = 'display-options-displays-view';
      displaysViewHost.appendChild(this.displaysView_);
    },

    /**
     * Lays out the display rectangles for mirroring.
     * @private
     */
    layoutMirroringDisplays_: function() {
      // Offset pixels for secondary display rectangles. The offset includes the
      // border width.
      /** @const */ var MIRRORING_OFFSET_PIXELS = 3;
      // Always show two displays because there must be two displays when
      // the display_options is enabled.  Don't rely on displays_.length because
      // there is only one display from chrome's perspective in mirror mode.
      /** @const */ var MIN_NUM_DISPLAYS = 2;
      /** @const */ var MIRRORING_VERTICAL_MARGIN = 20;

      // The width/height should be same as the first display:
      var width = Math.ceil(this.displays_[0].bounds.width * this.visualScale_);
      var height =
          Math.ceil(this.displays_[0].bounds.height * this.visualScale_);

      var numDisplays = Math.max(MIN_NUM_DISPLAYS, this.displays_.length);

      var totalWidth = width + numDisplays * MIRRORING_OFFSET_PIXELS;
      var totalHeight = height + numDisplays * MIRRORING_OFFSET_PIXELS;

      this.displaysView_.style.height = totalHeight + 'px';

      // The displays should be centered.
      var offsetX =
          $('display-options-displays-view').offsetWidth / 2 - totalWidth / 2;

      for (var i = 0; i < numDisplays; i++) {
        var div = /** @type {HTMLElement} */ (document.createElement('div'));
        div.className = 'displays-display';
        div.style.top = i * MIRRORING_OFFSET_PIXELS + 'px';
        div.style.left = i * MIRRORING_OFFSET_PIXELS + offsetX + 'px';
        div.style.width = width + 'px';
        div.style.height = height + 'px';
        div.style.zIndex = i;
        // set 'display-mirrored' class for the background display rectangles.
        if (i != numDisplays - 1)
          div.classList.add('display-mirrored');
        this.displaysView_.appendChild(div);
      }
    },

    /**
     * Creates a DisplayLayout object representing the display.
     * @param {!options.DisplayInfo} display
     * @param {!options.DisplayLayoutType} layoutType
     * @return {!options.DisplayLayout}
     * @private
     */
    createDisplayLayout_: function(display, layoutType) {
      return {
        bounds: display.bounds,
        div: null,
        id: display.id,
        isPrimary: display.isPrimary,
        layoutType: layoutType,
        name: display.name,
        originalPosition: {x: 0, y: 0}
      };
    },

    /**
     * Creates a div element representing the specified display.
     * @param {!options.DisplayLayout} displayLayout
     * @param {options.DisplayLayoutType} layoutType The layout type for the
     *     secondary display.
     * @param {!options.DisplayPosition} offset The offset to the center of the
     *     display area.
     * @private
     */
    createDisplayLayoutDiv_: function(displayLayout, layoutType, offset) {
      var div = /** @type {!HTMLElement} */ (document.createElement('div'));
      div.className = 'displays-display';
      div.classList.toggle(
          'displays-focused', displayLayout.id == this.focusedId_);

      // div needs to be added to the DOM tree first, otherwise offsetHeight for
      // nameContainer below cannot be computed.
      this.displaysView_.appendChild(div);

      var nameContainer = document.createElement('div');
      nameContainer.textContent = displayLayout.name;
      div.appendChild(nameContainer);

      var bounds = displayLayout.bounds;
      div.style.width = Math.floor(bounds.width * this.visualScale_) + 'px';
      var newHeight = Math.floor(bounds.height * this.visualScale_);
      div.style.height = newHeight + 'px';
      nameContainer.style.marginTop =
          (newHeight - nameContainer.offsetHeight) / 2 + 'px';

      div.onmousedown = this.onMouseDown_.bind(this);
      div.ontouchstart = this.onTouchStart_.bind(this);

      if (displayLayout.isPrimary) {
        div.style.left =
            Math.floor(bounds.left * this.visualScale_) + offset.x + 'px';
        div.style.top =
            Math.floor(bounds.top * this.visualScale_) + offset.y + 'px';
      } else {
        // Don't trust the secondary display's x or y, because it may cause a
        // 1px gap due to rounding, which will create a fake update on end
        // dragging. See crbug.com/386401
        var primaryDiv = this.displayLayoutMap_[this.primaryDisplayId_].div;
        switch (layoutType) {
          case options.DisplayLayoutType.TOP:
            div.style.left =
                Math.floor(bounds.left * this.visualScale_) + offset.x + 'px';
            div.style.top = primaryDiv.offsetTop - div.offsetHeight + 'px';
            break;
          case options.DisplayLayoutType.RIGHT:
            div.style.left =
                primaryDiv.offsetLeft + primaryDiv.offsetWidth + 'px';
            div.style.top =
                Math.floor(bounds.top * this.visualScale_) + offset.y + 'px';
            break;
          case options.DisplayLayoutType.BOTTOM:
            div.style.left =
                Math.floor(bounds.left * this.visualScale_) + offset.x + 'px';
            div.style.top =
                primaryDiv.offsetTop + primaryDiv.offsetHeight + 'px';
            break;
          case options.DisplayLayoutType.LEFT:
            div.style.left = primaryDiv.offsetLeft - div.offsetWidth + 'px';
            div.style.top =
                Math.floor(bounds.top * this.visualScale_) + offset.y + 'px';
            break;
        }
      }

      displayLayout.div = div;
      displayLayout.originalPosition.x = div.offsetLeft;
      displayLayout.originalPosition.y = div.offsetTop;
    },

    /**
     * Layouts the display rectangles according to the current layout_.
     * @param {options.DisplayLayoutType} layoutType
     * @private
     */
    layoutDisplays_: function(layoutType) {
      var maxWidth = 0;
      var maxHeight = 0;
      var boundingBox = {left: 0, right: 0, top: 0, bottom: 0};
      this.primaryDisplayId_ = '';
      this.secondaryDisplayId_ = '';
      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        if (display.isPrimary)
          this.primaryDisplayId_ = display.id;
        else if (this.secondaryDisplayId_ == '')
          this.secondaryDisplayId_ = display.id;

        var bounds = display.bounds;
        boundingBox.left = Math.min(boundingBox.left, bounds.left);
        boundingBox.right =
            Math.max(boundingBox.right, bounds.left + bounds.width);
        boundingBox.top = Math.min(boundingBox.top, bounds.top);
        boundingBox.bottom =
            Math.max(boundingBox.bottom, bounds.top + bounds.height);
        maxWidth = Math.max(maxWidth, bounds.width);
        maxHeight = Math.max(maxHeight, bounds.height);
      }
      if (this.primaryDisplayId_ == '')
        return;

      // Make the margin around the bounding box.
      var areaWidth = boundingBox.right - boundingBox.left + maxWidth;
      var areaHeight = boundingBox.bottom - boundingBox.top + maxHeight;

      // Calculates the scale by the width since horizontal size is more strict.
      // TODO(mukai): Adds the check of vertical size in case.
      this.visualScale_ = Math.min(
          VISUAL_SCALE, this.displaysView_.offsetWidth / areaWidth);

      // Prepare enough area for thisplays_view by adding the maximum height.
      this.displaysView_.style.height =
          Math.ceil(areaHeight * this.visualScale_) + 'px';

      // Centering the bounding box of the display rectangles.
      var offset = {
        x: Math.floor(
            this.displaysView_.offsetWidth / 2 -
            (boundingBox.right + boundingBox.left) * this.visualScale_ / 2),
        y: Math.floor(
            this.displaysView_.offsetHeight / 2 -
            (boundingBox.bottom + boundingBox.top) * this.visualScale_ / 2)
      };

      // Layout the display rectangles. First layout the primary display and
      // then layout any secondary displays.
      this.createDisplayLayoutDiv_(
          this.displayLayoutMap_[this.primaryDisplayId_], layoutType, offset);
      for (var i = 0; i < this.displays_.length; i++) {
        if (!this.displays_[i].isPrimary) {
          this.createDisplayLayoutDiv_(
              this.displayLayoutMap_[this.displays_[i].id], layoutType, offset);
        }
      }
    },

    /**
     * Called when the display arrangement has changed.
     * @param {options.MultiDisplayMode} mode multi display mode.
     * @param {!Array<!options.DisplayInfo>} displays The list of the display
     *     information.
     * @param {options.DisplayLayoutType} layoutType The layout strategy.
     * @param {number} offset The offset of the secondary display.
     * @private
     */
    onDisplayChanged_: function(mode, displays, layoutType, offset) {
      if (!this.visible)
        return;

      this.displays_ = displays;
      this.displayLayoutMap_ = {};
      for (var i = 0; i < displays.length; i++) {
        var display = displays[i];
        this.displayLayoutMap_[display.id] =
            this.createDisplayLayout_(display, layoutType);
      }

      var mirroring = mode == options.MultiDisplayMode.MIRRORING;
      var unifiedDesktopEnabled = mode == options.MultiDisplayMode.UNIFIED;

      // Focus to the first display next to the primary one when |displays_|
      // is updated.
      if (mirroring) {
        this.focusedId_ = '';
      } else if (
          this.focusedId_ == '' || this.mirroring_ != mirroring ||
          this.unifiedDesktopEnabled_ != unifiedDesktopEnabled ||
          this.displays_.length != displays.length) {
        this.focusedId_ = displays.length > 0 ? displays[0].id : '';
      }

      this.mirroring_ = mirroring;
      this.unifiedDesktopEnabled_ = unifiedDesktopEnabled;

      this.resetDisplaysView_();
      if (this.mirroring_)
        this.layoutMirroringDisplays_();
      else
        this.layoutDisplays_(layoutType);

      $('display-options-select-mirroring').value =
          mirroring ? 'mirroring' : 'extended';

      $('display-options-unified-desktop').hidden =
          !this.showUnifiedDesktopOption_;

      $('display-options-toggle-unified-desktop').checked =
          this.unifiedDesktopEnabled_;

      var disableUnifiedDesktopOption =
           (this.mirroring_ ||
            (!this.unifiedDesktopEnabled_ &&
              this.displays_.length == 1));

      $('display-options-toggle-unified-desktop').disabled =
          disableUnifiedDesktopOption;

      this.updateSelectedDisplayDescription_();
    }
  };

  DisplayOptions.setDisplayInfo = function(
      mode, displays, layoutType, offset) {
    DisplayOptions.getInstance().onDisplayChanged_(
        mode, displays, layoutType, offset);
  };

  // Export
  return {
    DisplayOptions: DisplayOptions
  };
});
