// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  // The scale ratio of the display rectangle to its original size.
  /** @const */ var VISUAL_SCALE = 1 / 10;

  // The number of pixels to share the edges between displays.
  /** @const */ var MIN_OFFSET_OVERLAP = 5;

  // The border width of the display rectangles for the focused one.
  /** @const */ var FOCUSED_BORDER_WIDTH_PX = 2;
  // The border width of the display rectangles for the normal one.
  /** @const */ var NORMAL_BORDER_WIDTH_PX = 1;

  // The constant values for the overscan calibration settings.
  // The height of an arrow.
  /** @const */ var ARROW_SIZE_PX = 10;

  // The gap from the boundary of the display rectangle and the arrow.
  /** @const */ var ARROW_GAP_PX = 2;

  // The margin size to handle events outside the target display.
  /** @const */ var ARROW_CONTAINER_MARGIN_PX = ARROW_SIZE_PX * 3;

  // The interval times to update the overscan while the user keeps pressing
  // the mouse button or touching.
  /** @const */ var OVERSCAN_TIC_INTERVAL_MS = 100;

  /**
   * Enumeration of secondary display layout.  The value has to be same as the
   * values in ash/display/display_controller.cc.
   * @enum {number}
   */
  var SecondaryDisplayLayout = {
    TOP: 0,
    RIGHT: 1,
    BOTTOM: 2,
    LEFT: 3
  };

  /**
   * Enumeration of the direction for the calibrating overscan settings.
   * @enum {number}
   */
  var CalibrationDirection = {
      INNER: 1,
      OUTER: -1
  };

  /**
   * Calculates the bounds of |element| relative to the page.
   * @param {HTMLElement} element The element to be known.
   * @return {Object} The object for the bounds, with x, y, width, and height.
   */
  function getBoundsInPage(element) {
    var bounds = {
      x: element.offsetLeft,
      y: element.offsetTop,
      width: element.offsetWidth,
      height: element.offsetHeight
    };
    var parent = element.offsetParent;
    while (parent && parent != document.body) {
      bounds.x += parent.offsetLeft;
      bounds.y += parent.offsetTop;
      parent = parent.offsetParent;
    }
    return bounds;
  }

  /**
   * Gets the position of |point| to |rect|, left, right, top, or bottom.
   * @param {Object} rect The base rectangle with x, y, width, and height.
   * @param {Object} point The point to check the position.
   * @return {SecondaryDisplayLayout} The position of the calculated point.
   */
  function getPositionToRectangle(rect, point) {
    // Separates the area into four (LEFT/RIGHT/TOP/BOTTOM) by the diagonals of
    // the rect, and decides which area the display should reside.
    var diagonalSlope = rect.height / rect.width;
    var topDownIntercept = rect.y - rect.x * diagonalSlope;
    var bottomUpIntercept = rect.y + rect.height + rect.x * diagonalSlope;

    if (point.y > topDownIntercept + point.x * diagonalSlope) {
      if (point.y > bottomUpIntercept - point.x * diagonalSlope)
        return SecondaryDisplayLayout.BOTTOM;
      else
        return SecondaryDisplayLayout.LEFT;
    } else {
      if (point.y > bottomUpIntercept - point.x * diagonalSlope)
        return SecondaryDisplayLayout.RIGHT;
      else
        return SecondaryDisplayLayout.TOP;
    }
  }

  /**
   * DisplayOverscanCalibrator shows the arrows to calibrate overscan settings
   * and handles events of the actual user control.
   * @param {Object} display The display object for the calibrating overscan.
   * @constructor
   */
  function DisplayOverscanCalibrator(display) {
    // Creates the calibration arrows over |display|.  To achieve the UI,
    // a transparent container holds the arrows and handles events.
    this.container_ = document.createElement('div');
    this.container_.id = 'display-overscan-calibration-arrow-container';
    var containerSize = {
      width: display.div.offsetWidth + ARROW_CONTAINER_MARGIN_PX * 2,
      height: display.div.offsetHeight + ARROW_CONTAINER_MARGIN_PX * 2
    };
    this.container_.style.width = containerSize.width + 'px';
    this.container_.style.height = containerSize.height + 'px';
    this.container_.style.left =
        display.div.offsetLeft - ARROW_CONTAINER_MARGIN_PX + 'px';
    this.container_.style.top =
        display.div.offsetTop - ARROW_CONTAINER_MARGIN_PX + 'px';
    this.container_.onmousedown = this.onMouseDown_.bind(this);
    this.container_.onmouseup = this.onOperationEnd_.bind(this);
    this.container_.ontouchstart = this.onTouchStart_.bind(this);
    this.container_.ontouchend = this.onOperationEnd_.bind(this);

    // Creates arrows for each direction.
    var topArrow = this.createVerticalArrows_();
    topArrow.style.left = containerSize.width / 2 - ARROW_SIZE_PX + 'px';
    topArrow.style.top = ARROW_CONTAINER_MARGIN_PX -
        ARROW_SIZE_PX - ARROW_GAP_PX + 'px';
    this.container_.appendChild(topArrow);
    var bottomArrow = this.createVerticalArrows_();
    bottomArrow.style.left = containerSize.width / 2 - ARROW_SIZE_PX + 'px';
    bottomArrow.style.bottom = ARROW_CONTAINER_MARGIN_PX -
        ARROW_SIZE_PX - ARROW_GAP_PX + 'px';
    this.container_.appendChild(bottomArrow);
    var leftArrow = this.createHorizontalArrows_();
    leftArrow.style.left = ARROW_CONTAINER_MARGIN_PX -
        ARROW_SIZE_PX - ARROW_GAP_PX + 'px';
    leftArrow.style.top = containerSize.height / 2 - ARROW_SIZE_PX + 'px';
    this.container_.appendChild(leftArrow);
    var rightArrow = this.createHorizontalArrows_();
    rightArrow.style.right = ARROW_CONTAINER_MARGIN_PX -
        ARROW_SIZE_PX - ARROW_GAP_PX + 'px';
    rightArrow.style.top = containerSize.height / 2 - ARROW_SIZE_PX + 'px';
    this.container_.appendChild(rightArrow);

    display.div.parentNode.appendChild(this.container_);
    this.displayBounds_ = getBoundsInPage(display.div);
    this.overscan_ = display.overscan;
    chrome.send('startOverscanCalibration', [display.id]);
  };

  DisplayOverscanCalibrator.prototype = {
    /**
     * The container of arrows.  It also receives the user events.
     * @private
     */
    container_: null,

    /**
     * The bounds of the display rectangle in the page.
     * @private
     */
    displayBounds_: null,

    /**
     * The current overscan settins.
     * @private
     */
    overscan_: null,

    /**
     * The location of the current user operation against the display.  The
     * contents should be one of 'left', 'right', 'top', or 'bottom'.
     * @type {string}
     * @private
     */
    location_: null,

    /**
     * The direction of the current user operation against the display.
     * @type {CalibrationDirection}
     * @private
     */
    direction_: null,

    /**
     * The ID for the periodic timer to tic the calibration settings while the
     * user keeps pressing the mouse button.
     * @type {number}
     * @private
     */
    timer_: null,

    /**
     * Called when everything is finished.
     */
    finish: function() {
      this.container_.parentNode.removeChild(this.container_);
      chrome.send('finishOverscanCalibration');
    },

    /**
     * Called when every settings are cleared.
     */
    clear: function() {
      chrome.send('clearOverscanCalibration');
    },

    /**
     * Mouse down event handler for overscan calibration.
     * @param {Event} e The mouse down event.
     * @private
     */
    onMouseDown_: function(e) {
      e.preventDefault();
      this.setupOverscanCalibration_(e);
    },

    /**
     * Touch start event handler for overscan calibration.
     * @param {Event} e The touch start event.
     * @private
     */
    onTouchStart_: function(e) {
      if (e.touches.length != 1)
        return;

      e.preventDefault();
      var touch = e.touches[0];
      this.setupOverscanCalibration_(e.touches[0]);
    },

    /**
     * Event handler for ending the user operation of overscan calibration.
     * @param {Event} e The event object, mouse up event or touch end event.
     * @private
     */
    onOperationEnd_: function(e) {
      if (this.timer_) {
        window.clearInterval(this.timer_);
        this.timer_ = null;
        e.preventDefault();
      }
    },

    /**
     * Sets up a new overscan calibration operation.  It calculates the event
     * location and determines the contents of location.  It also sets up a
     * timer to change the overscan settings continuously as far as the user
     * keeps pressing the mouse button or touching.  The timer will be cleared
     * on ending the operation.
     * @param {Object} e The object to contain the location of the event with
     *     pageX and pageY attributes.
     * @private
     */
    setupOverscanCalibration_: function(e) {
      switch (getPositionToRectangle(
          this.displayBounds_, {x: e.pageX, y: e.pageY})) {
      case SecondaryDisplayLayout.RIGHT:
        this.location_ = 'right';
        if (e.pageX < this.displayBounds_.x + this.displayBounds_.width)
          this.direction_ = CalibrationDirection.INNER;
        else
          this.direction_ = CalibrationDirection.OUTER;
        break;
      case SecondaryDisplayLayout.LEFT:
        this.location_ = 'left';
        if (e.pageX > this.displayBounds_.x)
          this.direction_ = CalibrationDirection.INNER;
        else
          this.direction_ = CalibrationDirection.OUTER;
        break;
      case SecondaryDisplayLayout.TOP:
        this.location_ = 'top';
        if (e.pageY > this.displayBounds_.y)
          this.direction_ = CalibrationDirection.INNER;
        else
          this.direction_ = CalibrationDirection.OUTER;
        break;
      case SecondaryDisplayLayout.BOTTOM:
        this.location_ = 'bottom';
        if (e.pageY < this.displayBounds_.y + this.displayBounds_.height) {
          this.direction_ = CalibrationDirection.INNER;
        } else {
          this.direction_ = CalibrationDirection.OUTER;
        }
        break;
      }

      this.ticOverscanSize_();
      this.timer_ = window.setInterval(
          this.ticOverscanSize_.bind(this), OVERSCAN_TIC_INTERVAL_MS);
    },

    /**
     * Modifies the current overscans actually and sends the update to the
     * system.
     * @private
     */
    ticOverscanSize_: function() {
      // Ignore the operation which causes some of overscan insets negative.
      if (this.direction_ == CalibrationDirection.OUTER &&
          this.overscan_[this.location_] == 0) {
        return;
      }

      this.overscan_[this.location_] += this.direction_;
      chrome.send('updateOverscanCalibration',
                  [this.overscan_.top, this.overscan_.left,
                   this.overscan_.bottom, this.overscan_.right]);
    },

    /**
     * Creates the arrows vertically aligned, for the calibration UI at the top
     * and bottom of the target display.
     * @return {HTMLElement} The created div which contains the arrows.
     * @private
     */
    createVerticalArrows_: function() {
      var container = document.createElement('div');
      container.style.width = ARROW_SIZE_PX * 2 + 'px';
      container.style.height =
          ARROW_SIZE_PX * 2 + ARROW_GAP_PX * 2 + FOCUSED_BORDER_WIDTH_PX + 'px';
      container.style.position = 'absolute';

      var arrowUp = document.createElement('div');
      arrowUp.className = 'display-overscan-calibration-arrow ' +
          'display-overscan-arrow-to-top';
      arrowUp.style.left = '0';
      arrowUp.style.top = -ARROW_SIZE_PX + 'px';
      container.appendChild(arrowUp);
      var arrowDown = document.createElement('div');
      arrowDown.className = 'display-overscan-calibration-arrow ' +
          'display-overscan-arrow-to-bottom';
      arrowDown.style.left = '0';
      arrowDown.style.bottom = -ARROW_SIZE_PX + 'px';
      container.appendChild(arrowDown);
      return container;
    },

    /**
     * Creates the arrows horizontally aligned, for the calibration UI at the
     * left and right of the target display.
     * @return {HTMLElement} The created div which contains the arrows.
     * @private
     */
    createHorizontalArrows_: function() {
      var container = document.createElement('div');
      container.style.width =
          ARROW_SIZE_PX * 2 + ARROW_GAP_PX * 2 + FOCUSED_BORDER_WIDTH_PX + 'px';
      container.style.height = ARROW_SIZE_PX * 2 + 'px';
      container.style.position = 'absolute';

      var arrowLeft = document.createElement('div');
      arrowLeft.className = 'display-overscan-calibration-arrow ' +
          'display-overscan-arrow-to-left';
      arrowLeft.style.left = -ARROW_SIZE_PX + 'px';
      arrowLeft.style.top = '0';
      container.appendChild(arrowLeft);
      var arrowRight = document.createElement('div');
      arrowRight.className = 'display-overscan-calibration-arrow ' +
          'display-overscan-arrow-to-right';
      arrowRight.style.right = -ARROW_SIZE_PX + 'px';
      arrowRight.style.top = '0';
      container.appendChild(arrowRight);
      return container;
    }
  };

  /**
   * Encapsulated handling of the 'Display' page.
   * @constructor
   */
  function DisplayOptions() {
    OptionsPage.call(this, 'display',
                     loadTimeData.getString('displayOptionsPageTabTitle'),
                     'display-options-page');
  }

  cr.addSingletonGetter(DisplayOptions);

  DisplayOptions.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Whether the current output status is mirroring displays or not.
     * @private
     */
    mirroring_: false,

    /**
     * The current secondary display layout.
     * @private
     */
    layout_: SecondaryDisplayLayout.RIGHT,

    /**
     * The array of current output displays.  It also contains the display
     * rectangles currently rendered on screen.
     * @private
     */
    displays_: [],

    /**
     * The index for the currently focused display in the options UI.  null if
     * no one has focus.
     * @private
     */
    focusedIndex_: null,

    /**
     * The primary display.
     * @private
     */
    primaryDisplay_: null,

    /**
     * The secondary display.
     * @private
     */
    secondaryDisplay_: null,

    /**
     * The container div element which contains all of the display rectangles.
     * @private
     */
    displaysView_: null,

    /**
     * The scale factor of the actual display size to the drawn display
     * rectangle size.
     * @private
     */
    visualScale_: VISUAL_SCALE,

    /**
     * The location where the last touch event happened.  This is used to
     * prevent unnecessary dragging events happen.  Set to null unless it's
     * during touch events.
     * @private
     */
    lastTouchLocation_: null,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('display-options-toggle-mirroring').onclick = (function() {
        this.mirroring_ = !this.mirroring_;
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

      $('selected-display-start-calibrating-overscan').onclick = (function() {
        this.overscanCalibrator_ = new DisplayOverscanCalibrator(
            this.displays_[this.focusedIndex_]);
        this.updateSelectedDisplayDescription_();
      }).bind(this);
      $('selected-display-finish-calibrating-overscan').onclick = (function() {
        this.overscanCalibrator_.finish();
        this.overscanCalibrator_ = null;
        this.updateSelectedDisplayDescription_();
      }).bind(this);
      $('selected-display-clear-calibrating-overscan').onclick = (function() {
        this.overscanCalibrator_.clear();
        this.overscanCalibrator_ = null;
        this.updateSelectedDisplayDescription_();
      }).bind(this);

      chrome.send('getDisplayInfo');
    },

    /** @override */
    onVisibilityChanged_: function() {
      OptionsPage.prototype.onVisibilityChanged_(this);
      if (this.visible)
        chrome.send('getDisplayInfo');
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
      var x_diff = Math.abs(touchLocation.x - this.lastTouchLocation_.x);
      var y_diff = Math.abs(touchLocation.y - this.lastTouchLocation_.y);
      if (x_diff <= IGNORABLE_TOUCH_MOVE_PX &&
          y_diff <= IGNORABLE_TOUCH_MOVE_PX) {
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
      return this.startDragging_(e.target, {x: e.pageX, y: e.pageY});
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
      return this.startDragging_(e.target, this.lastTouchLocation_);
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
      if (this.layout_ == SecondaryDisplayLayout.LEFT ||
          this.layout_ == SecondaryDisplayLayout.RIGHT) {
        offset = secondary.div.offsetTop - primary.div.offsetTop;
      } else {
        offset = secondary.div.offsetLeft - primary.div.offsetLeft;
      }
      chrome.send('setDisplayLayout',
                  [this.layout_, offset / this.visualScale_]);
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
     * @param {Object} eventLocation The location where the event happens.
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

      var newCenter = {
        x: newPosition.x + draggingDiv.offsetWidth / 2,
        y: newPosition.y + draggingDiv.offsetHeight / 2
      };

      var baseBounds = {
        x: baseDiv.offsetLeft,
        y: baseDiv.offsetTop,
        width: baseDiv.offsetWidth,
        height: baseDiv.offsetHeight
      };
      switch (getPositionToRectangle(baseBounds, newCenter)) {
      case SecondaryDisplayLayout.RIGHT:
        this.layout_ = this.dragging_.display.isPrimary ?
            SecondaryDisplayLayout.LEFT : SecondaryDisplayLayout.RIGHT;
        break;
      case SecondaryDisplayLayout.LEFT:
        this.layout_ = this.dragging_.display.isPrimary ?
            SecondaryDisplayLayout.RIGHT : SecondaryDisplayLayout.LEFT;
        break;
      case SecondaryDisplayLayout.TOP:
        this.layout_ = this.dragging_.display.isPrimary ?
            SecondaryDisplayLayout.BOTTOM : SecondaryDisplayLayout.TOP;
        break;
      case SecondaryDisplayLayout.BOTTOM:
        this.layout_ = this.dragging_.display.isPrimary ?
            SecondaryDisplayLayout.TOP : SecondaryDisplayLayout.BOTTOM;
        break;
      }

      if (this.layout_ == SecondaryDisplayLayout.LEFT ||
          this.layout_ == SecondaryDisplayLayout.RIGHT) {
        if (newPosition.y > baseDiv.offsetTop + baseDiv.offsetHeight)
          this.layout_ = this.dragging_.display.isPrimary ?
              SecondaryDisplayLayout.TOP : SecondaryDisplayLayout.BOTTOM;
        else if (newPosition.y + draggingDiv.offsetHeight <
                 baseDiv.offsetTop)
          this.layout_ = this.dragging_.display.isPrimary ?
              SecondaryDisplayLayout.BOTTOM : SecondaryDisplayLayout.TOP;
      } else {
        if (newPosition.y > baseDiv.offsetLeft + baseDiv.offsetWidth)
          this.layout_ = this.dragging_.display.isPrimary ?
              SecondaryDisplayLayout.LEFT : SecondaryDisplayLayout.RIGHT;
        else if (newPosition.y + draggingDiv.offsetWidth <
                   baseDiv.offstLeft)
          this.layout_ = this.dragging_.display.isPrimary ?
              SecondaryDisplayLayout.RIGHT : SecondaryDisplayLayout.LEFT;
      }

      var layout_to_base;
      if (!this.dragging_.display.isPrimary) {
        layout_to_base = this.layout_;
      } else {
        switch (this.layout_) {
        case SecondaryDisplayLayout.RIGHT:
          layout_to_base = SecondaryDisplayLayout.LEFT;
          break;
        case SecondaryDisplayLayout.LEFT:
          layout_to_base = SecondaryDisplayLayout.RIGHT;
          break;
        case SecondaryDisplayLayout.TOP:
          layout_to_base = SecondaryDisplayLayout.BOTTOM;
          break;
        case SecondaryDisplayLayout.BOTTOM:
          layout_to_base = SecondaryDisplayLayout.TOP;
          break;
        }
      }

      switch (layout_to_base) {
      case SecondaryDisplayLayout.RIGHT:
        draggingDiv.style.left =
            baseDiv.offsetLeft + baseDiv.offsetWidth + 'px';
        draggingDiv.style.top = newPosition.y + 'px';
        break;
      case SecondaryDisplayLayout.LEFT:
        draggingDiv.style.left =
            baseDiv.offsetLeft - draggingDiv.offsetWidth + 'px';
        draggingDiv.style.top = newPosition.y + 'px';
        break;
      case SecondaryDisplayLayout.TOP:
        draggingDiv.style.top =
            baseDiv.offsetTop - draggingDiv.offsetHeight + 'px';
        draggingDiv.style.left = newPosition.x + 'px';
        break;
      case SecondaryDisplayLayout.BOTTOM:
        draggingDiv.style.top =
            baseDiv.offsetTop + baseDiv.offsetHeight + 'px';
        draggingDiv.style.left = newPosition.x + 'px';
        break;
      }

      return false;
    },

    /**
     * start dragging of a display rectangle.
     * @param {HTMLElement} target The event target.
     * @param {Object} eventLocation The object to hold the location where
     *     this event happens.
     * @private
     */
    startDragging_: function(target, eventLocation) {
      this.focusedIndex_ = null;
      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        if (display.div == target ||
            (target.offsetParent && target.offsetParent == display.div)) {
          this.focusedIndex_ = i;
          break;
        }
      }

      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        display.div.className = 'displays-display';
        this.resizeDisplayRectangle_(display, i);
        if (i != this.focusedIndex_)
          continue;

        display.div.classList.add('displays-focused');
        this.dragging_ = {
          display: display,
          originalLocation: {
            x: display.div.offsetLeft, y: display.div.offsetTop
          },
          eventLocation: eventLocation
        };
      }

      if (this.overscanCalibrator_) {
        this.overscanCalibrator_.finish();
        this.overscanCalibrator_ = null;
      }
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
            this.secondaryDisplay_.div : this.primaryDisplay_.div;
        var draggingDiv = this.dragging_.display.div;
        if (this.layout_ == SecondaryDisplayLayout.LEFT ||
            this.layout_ == SecondaryDisplayLayout.RIGHT) {
          var top = Math.max(draggingDiv.offsetTop,
                             baseDiv.offsetTop - draggingDiv.offsetHeight +
                             MIN_OFFSET_OVERLAP);
          top = Math.min(top,
                         baseDiv.offsetTop + baseDiv.offsetHeight -
                         MIN_OFFSET_OVERLAP);
          draggingDiv.style.top = top + 'px';
        } else {
          var left = Math.max(draggingDiv.offsetLeft,
                              baseDiv.offsetLeft - draggingDiv.offsetWidth +
                              MIN_OFFSET_OVERLAP);
          left = Math.min(left,
                          baseDiv.offsetLeft + baseDiv.offsetWidth -
                          MIN_OFFSET_OVERLAP);
          draggingDiv.style.left = left + 'px';
        }
        var originalPosition = this.dragging_.display.originalPosition;
        if (originalPosition.x != draggingDiv.offsetLeft ||
            originalPosition.y != draggingDiv.offsetTop)
          this.applyResult_();
        this.dragging_ = null;
      }
      this.updateSelectedDisplayDescription_();
      return false;
    },

    /**
     * Updates the description of the selected display section.
     * @private
     */
    updateSelectedDisplayDescription_: function() {
      if (this.focusedIndex_ == null ||
          this.displays_[this.focusedIndex_] == null) {
        $('selected-display-data-container').hidden = true;
        $('display-configuration-arrow').hidden = true;
        $('display-options-set-primary').hidden = true;
        $('display-options-toggle-mirroring').hidden = true;
        return;
      }

      $('selected-display-data-container').hidden = false;
      var display = this.displays_[this.focusedIndex_];
      var nameElement = $('selected-display-name');
      while (nameElement.childNodes.length > 0)
        nameElement.removeChild(nameElement.firstChild);
      nameElement.appendChild(document.createTextNode(display.name));

      var resolutionData = display.width + 'x' + display.height;
      var resolutionElement = $('selected-display-resolution');
      while (resolutionElement.childNodes.length > 0)
        resolutionElement.removeChild(resolutionElement.firstChild);
      resolutionElement.appendChild(document.createTextNode(resolutionData));

      if (display.isInternal) {
        $('start-calibrating-overscan-control').hidden = true;
        $('end-calibrating-overscan-control').hidden = true;
      } else if (this.overscanCalibrator_) {
        $('start-calibrating-overscan-control').hidden = true;
        $('end-calibrating-overscan-control').hidden = false;
      } else {
        $('start-calibrating-overscan-control').hidden = false;
        $('end-calibrating-overscan-control').hidden = true;
      }

      var arrow = $('display-configuration-arrow');
      arrow.hidden = false;
      // Adding 1 px to the position to fit the border line and the border in
      // arrow precisely.
      arrow.style.top = $('display-configurations').offsetTop -
          arrow.offsetHeight / 2 + 1 + 'px';
      arrow.style.left = display.div.offsetLeft + display.div.offsetWidth / 2 -
          arrow.offsetWidth / 2 + 'px';

      $('display-options-set-primary').hidden =
          this.displays_[this.focusedIndex_].isPrimary;
      $('display-options-toggle-mirroring').hidden =
          (this.displays_.length <= 1);
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
     * Resize the specified display rectangle to keep the change of
     * the border width.
     * @param {Object} display The display object.
     * @param {number} index The index of the display.
     * @private
     */
    resizeDisplayRectangle_: function(display, index) {
      var borderWidth = (index == this.focusedIndex_) ?
          FOCUSED_BORDER_WIDTH_PX : NORMAL_BORDER_WIDTH_PX;
      display.div.style.width =
          display.width * this.visualScale_ - borderWidth * 2 + 'px';
      var newHeight = display.height * this.visualScale_ - borderWidth * 2;
      display.div.style.height = newHeight + 'px';
      display.nameContainer.style.marginTop =
            (newHeight - display.nameContainer.offsetHeight) / 2 + 'px';
      if (display.isPrimary) {
        var launcher = display.div.firstChild;
        if (launcher && launcher.id == 'display-launcher') {
          launcher.style.width = display.div.style.width;
        }
      }
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
      var width = this.displays_[0].width * this.visualScale_;
      var height = this.displays_[0].height * this.visualScale_;

      var numDisplays = Math.max(MIN_NUM_DISPLAYS, this.displays_.length);

      var totalWidth = width + numDisplays * MIRRORING_OFFSET_PIXELS;
      var totalHeight = height + numDisplays * MIRRORING_OFFSET_PIXELS;

      this.displaysView_.style.height = totalHeight + 'px';
      this.displaysView_.classList.add(
          'display-options-displays-view-mirroring');

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
     * Layouts the display rectangles according to the current layout_.
     * @private
     */
    layoutDisplays_: function() {
      var maxWidth = 0;
      var maxHeight = 0;
      var boundingBox = {left: 0, right: 0, top: 0, bottom: 0};
      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        boundingBox.left = Math.min(boundingBox.left, display.x);
        boundingBox.right = Math.max(
            boundingBox.right, display.x + display.width);
        boundingBox.top = Math.min(boundingBox.top, display.y);
        boundingBox.bottom = Math.max(
            boundingBox.bottom, display.y + display.height);
        maxWidth = Math.max(maxWidth, display.width);
        maxHeight = Math.max(maxHeight, display.height);
      }

      // Make the margin around the bounding box.
      var areaWidth = boundingBox.right - boundingBox.left + maxWidth;
      var areaHeight = boundingBox.bottom - boundingBox.top + maxHeight;

      // Calculates the scale by the width since horizontal size is more strict.
      // TODO(mukai): Adds the check of vertical size in case.
      this.visualScale_ = Math.min(
          VISUAL_SCALE, this.displaysView_.offsetWidth / areaWidth);

      // Prepare enough area for thisplays_view by adding the maximum height.
      this.displaysView_.style.height = areaHeight * this.visualScale_ + 'px';

      var boundingCenter = {
        x: (boundingBox.right + boundingBox.left) * this.visualScale_ / 2,
        y: (boundingBox.bottom + boundingBox.top) * this.visualScale_ / 2};

      // Centering the bounding box of the display rectangles.
      var offset = {
        x: this.displaysView_.offsetWidth / 2 -
           (boundingBox.right + boundingBox.left) * this.visualScale_ / 2,
        y: this.displaysView_.offsetHeight / 2 -
           (boundingBox.bottom + boundingBox.top) * this.visualScale_ / 2};

      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        var div = document.createElement('div');
        display.div = div;

        div.className = 'displays-display';
        if (i == this.focusedIndex_)
          div.classList.add('displays-focused');

        if (display.isPrimary) {
          // Put a grey rectangle to the primary display to denote launcher
          // below.
          var launcher = document.createElement('div');
          launcher.id = 'display-launcher';
          div.appendChild(launcher);
          this.primaryDisplay_ = display;
        } else {
          this.secondaryDisplay_ = display;
        }
        var displayNameContainer = document.createElement('div');
        displayNameContainer.textContent = display.name;
        div.appendChild(displayNameContainer);
        display.nameContainer = displayNameContainer;
        this.resizeDisplayRectangle_(display, i);
        div.style.left = display.x * this.visualScale_ + offset.x + 'px';
        div.style.top = display.y * this.visualScale_ + offset.y + 'px';

        div.onmousedown = this.onMouseDown_.bind(this);
        div.ontouchstart = this.onTouchStart_.bind(this);

        this.displaysView_.appendChild(div);

        // Set the margin top to place the display name at the middle of the
        // rectangle.  Note that this has to be done after it's added into the
        // |displaysView_|.  Otherwise its offsetHeight is yet 0.
        displayNameContainer.style.marginTop =
            (div.offsetHeight - displayNameContainer.offsetHeight) / 2 + 'px';
        display.originalPosition = {x: div.offsetLeft, y: div.offsetTop};
      }
    },

    /**
     * Called when the display arrangement has changed.
     * @param {boolean} mirroring Whether current mode is mirroring or not.
     * @param {Array} displays The list of the display information.
     * @param {SecondaryDisplayLayout} layout The layout strategy.
     * @param {number} offset The offset of the secondary display.
     * @private
     */
    onDisplayChanged_: function(mirroring, displays, layout, offset) {
      if (displays.length <= 1) {
        OptionsPage.showDefaultPage();
        return;
      }

      this.mirroring_ = mirroring;
      this.layout_ = layout;
      this.offset_ = offset;
      this.dirty_ = false;

      $('display-options-toggle-mirroring').textContent =
          loadTimeData.getString(
              this.mirroring_ ? 'stopMirroring' : 'startMirroring');

      // Focus to the first display next to the primary one when |displays| list
      // is updated.
      if (this.mirroring_)
        this.focusedIndex_ = null;
      else if (this.displays_.length != displays.length)
        this.focusedIndex_ = 1;

      this.displays_ = displays;

      this.resetDisplaysView_();
      if (this.mirroring_)
        this.layoutMirroringDisplays_();
      else
        this.layoutDisplays_();

      if (this.overscanCalibrator_) {
        this.overscanCalibrator_.finish();
        this.overscanCalibrator_ = new DisplayOverscanCalibrator(
            this.displays_[this.focusedIndex_]);
      }
      this.updateSelectedDisplayDescription_();
    }
  };

  DisplayOptions.setDisplayInfo = function(
      mirroring, displays, layout, offset) {
    DisplayOptions.getInstance().onDisplayChanged_(
        mirroring, displays, layout, offset);
  };

  // Export
  return {
    DisplayOptions: DisplayOptions
  };
});
