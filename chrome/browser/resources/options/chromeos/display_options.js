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
 *   div: ?Element,
 *   id: string,
 *   isInternal: boolean,
 *   isPrimary: boolean,
 *   resolutions: !Array<!options.DisplayMode>,
 *   name: string,
 *   rotation: number,
 *   originalPosition: ?options.DisplayPosition
 * }}
 */
options.DisplayInfo;

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  // The scale ratio of the display rectangle to its original size.
  /** @const */ var VISUAL_SCALE = 1 / 10;

  // The number of pixels to share the edges between displays.
  /** @const */ var MIN_OFFSET_OVERLAP = 5;

  /**
   * Gets the position of |point| to |rect|, left, right, top, or bottom.
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
     * The current secondary display layout.
     * @type {options.DisplayLayoutType}
     * @private
     */
    layout_: options.DisplayLayoutType.RIGHT,

    /**
     * The array of current output displays.  It also contains the display
     * rectangles currently rendered on screen.
     * @type {!Array<!options.DisplayInfo>}
     * @private
     */
    displays_: [],

    /**
     * The index of the currently focused display, or -1 for none.
     * @type {number}
     * @private
     */
    focusedIndex_: -1,

    /**
     * The primary display edit info.
     * @type {?options.DisplayInfo}
     * @private
     */
    primaryDisplay_: null,

    /**
     * The secondary display edit info.
     * @type {?options.DisplayInfo}
     * @private
     */
    secondaryDisplay_: null,

    /**
     * Drag info.
     * @type {?{display: !options.DisplayInfo,
     *          originalLocation: !options.DisplayPosition,
     *          eventLocation: !options.DisplayPosition}}
     * @private
     */
    dragging_: null,

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
        chrome.send('setPrimary', [this.displays_[this.focusedIndex_].id]);
      }).bind(this);
      $('display-options-resolution-selection').onchange = (function(ev) {
        var display = this.displays_[this.focusedIndex_];
        var resolution = display.resolutions[ev.target.value];
        chrome.send('setDisplayMode', [display.id, resolution]);
      }).bind(this);
      $('display-options-orientation-selection').onchange = (function(ev) {
        var displayIndex =
          (this.focusedIndex_ == -1) ? 0 : this.focusedIndex_;
        var rotation = parseInt(ev.target.value, 10);
        chrome.send('setRotation', [this.displays_[displayIndex].id, rotation]);
      }).bind(this);
      $('display-options-color-profile-selection').onchange = (function(ev) {
        chrome.send('setColorProfile', [this.displays_[this.focusedIndex_].id,
                                        ev.target.value]);
      }).bind(this);
      $('selected-display-start-calibrating-overscan').onclick = (function() {
        // Passes the target display ID. Do not specify it through URL hash,
        // we do not care back/forward.
        var displayOverscan = options.DisplayOverscan.getInstance();
        displayOverscan.setDisplayId(this.displays_[this.focusedIndex_].id);
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
     * Collects the current data and sends it to Chrome.
     * @private
     */
    applyResult_: function() {
      // Offset is calculated from top or left edge.
      var primary = this.primaryDisplay_;
      var secondary = this.secondaryDisplay_;
      var offset;
      if (this.layout_ == options.DisplayLayoutType.LEFT ||
          this.layout_ == options.DisplayLayoutType.RIGHT) {
        offset = secondary.div.offsetTop - primary.div.offsetTop;
      } else {
        offset = secondary.div.offsetLeft - primary.div.offsetLeft;
      }
      offset = Math.floor(offset / this.visualScale_);
      chrome.send('setDisplayLayout', [secondary.id, this.layout_, offset]);
    },

    /**
     * Snaps the region [point, width] to [basePoint, baseWidth] if
     * the [point, width] is close enough to the base's edge.
     * @param {number} point The starting point of the region.
     * @param {number} width The width of the region.
     * @param {number} basePoint The starting point of the base region.
     * @param {number} baseWidth The width of the base region.
     * @return {number} The moved point.  Returns point itself if it doesn't
     *     need to snap to the edge.
     * @private
     */
    snapToEdge_: function(point, width, basePoint, baseWidth) {
      // If the edge of the regions is smaller than this, it will snap to the
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
    },

    /**
     * Processes the actual dragging of display rectangle.
     * @param {Event} e The event which triggers this drag.
     * @param {options.DisplayPosition} eventLocation The location where the
     *     event happens.
     * @private
     */
    processDragging_: function(e, eventLocation) {
      if (!this.dragging_)
        return true;

      var index = -1;
      for (var i = 0; i < this.displays_.length; i++) {
        if (this.displays_[i] == this.dragging_.display) {
          index = i;
          break;
        }
      }
      if (index < 0)
        return true;

      e.preventDefault();

      // Note that current code of moving display-rectangles doesn't work
      // if there are >=3 displays.  This is our assumption for M21.
      // TODO(mukai): Fix the code to allow >=3 displays.
      var newPosition = {
        x: this.dragging_.originalLocation.x +
            (eventLocation.x - this.dragging_.eventLocation.x),
        y: this.dragging_.originalLocation.y +
            (eventLocation.y - this.dragging_.eventLocation.y)
      };

      var baseDiv = this.dragging_.display.isPrimary ?
          this.secondaryDisplay_.div : this.primaryDisplay_.div;
      var draggingDiv = this.dragging_.display.div;

      newPosition.x = this.snapToEdge_(newPosition.x, draggingDiv.offsetWidth,
                                       baseDiv.offsetLeft, baseDiv.offsetWidth);
      newPosition.y = this.snapToEdge_(newPosition.y, draggingDiv.offsetHeight,
                                       baseDiv.offsetTop, baseDiv.offsetHeight);

      var newCenter =  /** {!options.DisplayPosition} */({
        x: newPosition.x + draggingDiv.offsetWidth / 2,
        y: newPosition.y + draggingDiv.offsetHeight / 2
      });

      var baseBounds = /** {!options.DisplayBounds} */({
        left: baseDiv.offsetLeft,
        top: baseDiv.offsetTop,
        width: baseDiv.offsetWidth,
        height: baseDiv.offsetHeight
      });
      switch (getPositionToRectangle(baseBounds, newCenter)) {
        case options.DisplayLayoutType.RIGHT:
          this.layout_ = this.dragging_.display.isPrimary ?
              options.DisplayLayoutType.LEFT :
              options.DisplayLayoutType.RIGHT;
          break;
        case options.DisplayLayoutType.LEFT:
          this.layout_ = this.dragging_.display.isPrimary ?
              options.DisplayLayoutType.RIGHT :
              options.DisplayLayoutType.LEFT;
          break;
        case options.DisplayLayoutType.TOP:
          this.layout_ = this.dragging_.display.isPrimary ?
              options.DisplayLayoutType.BOTTOM :
              options.DisplayLayoutType.TOP;
          break;
        case options.DisplayLayoutType.BOTTOM:
          this.layout_ = this.dragging_.display.isPrimary ?
              options.DisplayLayoutType.TOP :
              options.DisplayLayoutType.BOTTOM;
          break;
      }

      if (this.layout_ == options.DisplayLayoutType.LEFT ||
          this.layout_ == options.DisplayLayoutType.RIGHT) {
        if (newPosition.y > baseDiv.offsetTop + baseDiv.offsetHeight)
          this.layout_ = this.dragging_.display.isPrimary ?
              options.DisplayLayoutType.TOP :
              options.DisplayLayoutType.BOTTOM;
        else if (newPosition.y + draggingDiv.offsetHeight < baseDiv.offsetTop)
          this.layout_ = this.dragging_.display.isPrimary ?
              options.DisplayLayoutType.BOTTOM :
              options.DisplayLayoutType.TOP;
      } else {
        if (newPosition.x > baseDiv.offsetLeft + baseDiv.offsetWidth)
          this.layout_ = this.dragging_.display.isPrimary ?
              options.DisplayLayoutType.LEFT :
              options.DisplayLayoutType.RIGHT;
        else if (newPosition.x + draggingDiv.offsetWidth < baseDiv.offsetLeft)
          this.layout_ = this.dragging_.display.isPrimary ?
              options.DisplayLayoutType.RIGHT :
              options.DisplayLayoutType.LEFT;
      }

      var layoutToBase;
      if (!this.dragging_.display.isPrimary) {
        layoutToBase = this.layout_;
      } else {
        switch (this.layout_) {
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
      var oldFocusedIndex = this.focusedIndex_;
      var willUpdateDisplayDescription = false;
      this.focusedIndex_ = -1;
      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        if (display.div == target ||
            (target.offsetParent && target.offsetParent == display.div)) {
          this.focusedIndex_ = i;
          if (oldFocusedIndex !== null && oldFocusedIndex != i)
            willUpdateDisplayDescription = true;
          break;
        }
      }

      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        display.div.className = 'displays-display';
        if (i != this.focusedIndex_)
          continue;

        display.div.classList.add('displays-focused');
        if (this.displays_.length > 1) {
          this.dragging_ = {
            display: display,
            originalLocation: {
              x: display.div.offsetLeft, y: display.div.offsetTop
            },
            eventLocation: eventLocation
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
      if (this.dragging_) {
        // Make sure the dragging location is connected.
        var baseDiv = this.dragging_.display.isPrimary ?
            this.secondaryDisplay_.div :
            this.primaryDisplay_.div;
        var draggingDiv = this.dragging_.display.div;
        if (this.layout_ == options.DisplayLayoutType.LEFT ||
            this.layout_ == options.DisplayLayoutType.RIGHT) {
          var top = Math.max(
              draggingDiv.offsetTop, baseDiv.offsetTop -
                  draggingDiv.offsetHeight + MIN_OFFSET_OVERLAP);
          top = Math.min(
              top,
              baseDiv.offsetTop + baseDiv.offsetHeight - MIN_OFFSET_OVERLAP);
          draggingDiv.style.top = top + 'px';
        } else {
          var left = Math.max(
              draggingDiv.offsetLeft, baseDiv.offsetLeft -
                  draggingDiv.offsetWidth + MIN_OFFSET_OVERLAP);
          left = Math.min(
              left,
              baseDiv.offsetLeft + baseDiv.offsetWidth - MIN_OFFSET_OVERLAP);
          draggingDiv.style.left = left + 'px';
        }
        var originalPosition = this.dragging_.display.originalPosition;
        if (originalPosition.x != draggingDiv.offsetLeft ||
            originalPosition.y != draggingDiv.offsetTop)
          this.applyResult_();
        this.dragging_ = null;
      }
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
      var arrow = $('display-configuration-arrow');
      arrow.hidden = false;
      // Adding 1 px to the position to fit the border line and the border in
      // arrow precisely.
      arrow.style.top = $('display-configurations').offsetTop -
          arrow.offsetHeight / 2 + 'px';
      arrow.style.left = display.div.offsetLeft +
          display.div.offsetWidth / 2 - arrow.offsetWidth / 2 + 'px';

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
      } else if (this.focusedIndex_ == -1 ||
          this.displays_[this.focusedIndex_] == null) {
        this.updateSelectedDisplaySectionNoSelected_();
      } else {
        this.updateSelectedDisplaySectionForDisplay_(
            this.displays_[this.focusedIndex_]);
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
        var div = document.createElement('div');
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
     * Creates a div element representing the specified display.
     * @param {!options.DisplayInfo} display The display object.
     * @param {boolean} focused True if it's focused.
     * @private
     */
    createDisplayRectangle_: function(display, focused) {
      var div = document.createElement('div');
      display.div = div;
      div.className = 'displays-display';
      if (focused)
        div.classList.add('displays-focused');

      // div needs to be added to the DOM tree first, otherwise offsetHeight for
      // nameContainer below cannot be computed.
      this.displaysView_.appendChild(div);

      var nameContainer = document.createElement('div');
      nameContainer.textContent = display.name;
      div.appendChild(nameContainer);
      div.style.width =
          Math.floor(display.bounds.width * this.visualScale_) + 'px';
      var newHeight = Math.floor(display.bounds.height * this.visualScale_);
      div.style.height = newHeight + 'px';
      nameContainer.style.marginTop =
          (newHeight - nameContainer.offsetHeight) / 2 + 'px';

      div.onmousedown = this.onMouseDown_.bind(this);
      div.ontouchstart = this.onTouchStart_.bind(this);
      return div;
    },

    /**
     * Layouts the display rectangles according to the current layout_.
     * @private
     */
    layoutDisplays_: function() {
      var maxWidth = 0;
      var maxHeight = 0;
      var boundingBox = {left: 0, right: 0, top: 0, bottom: 0};
      this.primaryDisplay_ = null;
      this.secondaryDisplay_ = null;
      var focusedDisplay = null;
      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        if (display.isPrimary)
          this.primaryDisplay_ = display;
        else
          this.secondaryDisplay_ = display;
        if (i == this.focusedIndex_)
          focusedDisplay = display;

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
      if (!this.primaryDisplay_)
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

      // Layouting the display rectangles. First layout the primaryDisplay and
      // then layout the secondary which is attaching to the primary.
      var primaryDiv = this.createDisplayRectangle_(
          this.primaryDisplay_, this.primaryDisplay_ == focusedDisplay);
      primaryDiv.style.left =
          Math.floor(this.primaryDisplay_.bounds.left * this.visualScale_) +
          offset.x + 'px';
      primaryDiv.style.top =
          Math.floor(this.primaryDisplay_.bounds.top * this.visualScale_) +
          offset.y + 'px';
      this.primaryDisplay_.originalPosition = {
        x: primaryDiv.offsetLeft,
        y: primaryDiv.offsetTop
      };

      if (this.secondaryDisplay_) {
        var secondaryDiv = this.createDisplayRectangle_(
            this.secondaryDisplay_, this.secondaryDisplay_ == focusedDisplay);
        // Don't trust the secondary display's x or y, because it may cause a
        // 1px gap due to rounding, which will create a fake update on end
        // dragging. See crbug.com/386401
        var bounds = this.secondaryDisplay_.bounds;
        switch (this.layout_) {
          case options.DisplayLayoutType.TOP:
            secondaryDiv.style.left =
                Math.floor(bounds.left * this.visualScale_) + offset.x + 'px';
            secondaryDiv.style.top =
                primaryDiv.offsetTop - secondaryDiv.offsetHeight + 'px';
            break;
          case options.DisplayLayoutType.RIGHT:
            secondaryDiv.style.left =
                primaryDiv.offsetLeft + primaryDiv.offsetWidth + 'px';
            secondaryDiv.style.top =
                Math.floor(bounds.top * this.visualScale_) + offset.y + 'px';
            break;
          case options.DisplayLayoutType.BOTTOM:
            secondaryDiv.style.left =
                Math.floor(bounds.left * this.visualScale_) + offset.x + 'px';
            secondaryDiv.style.top =
                primaryDiv.offsetTop + primaryDiv.offsetHeight + 'px';
            break;
          case options.DisplayLayoutType.LEFT:
            secondaryDiv.style.left =
                primaryDiv.offsetLeft - secondaryDiv.offsetWidth + 'px';
            secondaryDiv.style.top =
                Math.floor(bounds.top * this.visualScale_) + offset.y + 'px';
            break;
        }
        this.secondaryDisplay_.originalPosition = {
          x: secondaryDiv.offsetLeft,
          y: secondaryDiv.offsetTop
        };
      }
    },

    /**
     * Called when the display arrangement has changed.
     * @param {options.MultiDisplayMode} mode multi display mode.
     * @param {Array<options.DisplayInfo>} displays The list of the display
     *     information.
     * @param {options.DisplayLayoutType} layout The layout strategy.
     * @param {number} offset The offset of the secondary display.
     * @private
     */
    onDisplayChanged_: function(mode, displays, layout, offset) {
      if (!this.visible)
        return;

      var hasExternal = false;
      for (var i = 0; i < displays.length; i++) {
        if (!displays[i].isInternal) {
          hasExternal = true;
          break;
        }
      }

      this.layout_ = layout;

      var mirroring = mode == options.MultiDisplayMode.MIRRORING;
      var unifiedDesktopEnabled = mode == options.MultiDisplayMode.UNIFIED;

      // Focus to the first display next to the primary one when |displays| list
      // is updated.
      if (mirroring) {
        this.focusedIndex_ = -1;
      } else if (this.mirroring_ != mirroring ||
                 this.unifiedDesktopEnabled_ != unifiedDesktopEnabled ||
                 this.displays_.length != displays.length) {
        this.focusedIndex_ = 0;
      }

      this.mirroring_ = mirroring;
      this.unifiedDesktopEnabled_ = unifiedDesktopEnabled;
      this.displays_ = displays;

      this.resetDisplaysView_();
      if (this.mirroring_)
        this.layoutMirroringDisplays_();
      else
        this.layoutDisplays_();

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
      mode, displays, layout, offset) {
    DisplayOptions.getInstance().onDisplayChanged_(
        mode, displays, layout, offset);
  };

  // Export
  return {
    DisplayOptions: DisplayOptions
  };
});
