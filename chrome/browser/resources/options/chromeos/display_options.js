// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  // The scale ratio of the display rectangle to its original size.
  /** @const */ var VISUAL_SCALE = 1 / 10;

  // The number of pixels to share the edges between displays.
  /** @const */ var MIN_OFFSET_OVERLAP = 5;

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
     */
    focusedIndex_: null,

    /**
     * The flag to check if the current options status should be sent to the
     * system or not (unchanged).
     * @private
     */
    dirty_: false,

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
     * Initialize the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('display-options-toggle-mirroring').onclick = (function() {
        this.mirroring_ = !this.mirroring_;
        chrome.send('setMirroring', [this.mirroring_]);
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
     * Collects the current data and sends it to Chrome.
     * @private
     */
    applyResult_: function() {
      // Offset is calculated from top or left edge.
      var primary = this.displays_[0];
      var secondary = this.displays_[1];
      var offset;
      if (this.layout_ == SecondaryDisplayLayout.LEFT ||
          this.layout_ == SecondaryDisplayLayout.RIGHT) {
        offset = secondary.div.offsetTop - primary.div.offsetTop;
      } else {
        offset = secondary.div.offsetLeft - primary.div.offsetLeft;
      }
      chrome.send('setDisplayLayout',
                  [this.layout_, offset / this.visualScale_]);
      this.dirty_ = false;
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
     * Mouse move handler for dragging display rectangle.
     * @private
     * @param {Event} e The mouse move event.
     */
    onMouseMove_: function(e) {
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

      // Note that current code of moving display-rectangles doesn't work
      // if there are >=3 displays.  This is our assumption for M21.
      // TODO(mukai): Fix the code to allow >=3 displays.
      var mousePosition = {
        x: e.pageX - this.dragging_.offset.x,
        y: e.pageY - this.dragging_.offset.y
      };
      var newPosition = {
        x: mousePosition.x - this.dragging_.clickLocation.x,
        y: mousePosition.y - this.dragging_.clickLocation.y
      };

      var baseDiv = this.displays_[this.dragging_.isPrimary ? 1 : 0].div;
      var draggingDiv = this.dragging_.display.div;

      newPosition.x = this.snapToEdge_(newPosition.x, draggingDiv.offsetWidth,
                                       baseDiv.offsetLeft, baseDiv.offsetWidth);
      newPosition.y = this.snapToEdge_(newPosition.y, draggingDiv.offsetHeight,
                                       baseDiv.offsetTop, baseDiv.offsetHeight);

      // Separate the area into four (LEFT/RIGHT/TOP/BOTTOM) by the diagonals of
      // the primary display, and decide which area the display should reside.
      var diagonalSlope = baseDiv.offsetHeight / baseDiv.offsetWidth;
      var topDownIntercept =
          baseDiv.offsetTop - baseDiv.offsetLeft * diagonalSlope;
      var bottomUpIntercept = baseDiv.offsetTop +
          baseDiv.offsetHeight + baseDiv.offsetLeft * diagonalSlope;

      if (mousePosition.y >
          topDownIntercept + mousePosition.x * diagonalSlope) {
        if (mousePosition.y >
            bottomUpIntercept - mousePosition.x * diagonalSlope)
          this.layout_ = this.dragging_.isPrimary ?
              SecondaryDisplayLayout.TOP : SecondaryDisplayLayout.BOTTOM;
        else
          this.layout_ = this.dragging_.isPrimary ?
              SecondaryDisplayLayout.RIGHT : SecondaryDisplayLayout.LEFT;
      } else {
        if (mousePosition.y >
            bottomUpIntercept - mousePosition.x * diagonalSlope)
          this.layout_ = this.dragging_.isPrimary ?
              SecondaryDisplayLayout.LEFT : SecondaryDisplayLayout.RIGHT;
        else
          this.layout_ = this.dragging_.isPrimary ?
              SecondaryDisplayLayout.BOTTOM : SecondaryDisplayLayout.TOP;
      }

      if (this.layout_ == SecondaryDisplayLayout.LEFT ||
          this.layout_ == SecondaryDisplayLayout.RIGHT) {
        if (newPosition.y > baseDiv.offsetTop + baseDiv.offsetHeight)
          this.layout_ = this.dragging_.isPrimary ?
              SecondaryDisplayLayout.TOP : SecondaryDisplayLayout.BOTTOM;
        else if (newPosition.y + draggingDiv.offsetHeight <
                 baseDiv.offsetTop)
          this.layout_ = this.dragging_.isPrimary ?
              SecondaryDisplayLayout.BOTTOM : SecondaryDisplayLayout.TOP;
      } else {
        if (newPosition.y > baseDiv.offsetLeft + baseDiv.offsetWidth)
          this.layout_ = this.dragging_.isPrimary ?
              SecondaryDisplayLayout.LEFT : SecondaryDisplayLayout.RIGHT;
        else if (newPosition.y + draggingDiv.offsetWidth <
                   baseDiv.offstLeft)
          this.layout_ = this.dragging_.isPrimary ?
              SecondaryDisplayLayout.RIGHT : SecondaryDisplayLayout.LEFT;
      }

      var layout_to_base;
      if (!this.dragging_.isPrimary) {
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

      this.dirty_ = true;
      return false;
    },

    /**
     * Mouse down handler for dragging display rectangle.
     * @private
     * @param {Event} e The mouse down event.
     */
    onMouseDown_: function(e) {
      if (this.mirroring_)
        return true;

      if (e.button != 0)
        return true;

      this.focusedIndex_ = null;
      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        if (this.displays_[i].div == e.target ||
            (i == 0 && $('display-launcher') == e.target)) {
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
            isPrimary: i == 0,
            clickLocation: {x: e.offsetX, y: e.offsetY},
            offset: {x: e.pageX - e.offsetX - display.div.offsetLeft,
                     y: e.pageY - e.offsetY - display.div.offsetTop}
        };
      }
      this.updateSelectedDisplayDescription_();
      return false;
    },

    /**
     * Mouse up handler for dragging display rectangle.
     * @private
     * @param {Event} e The mouse up event.
     */
    onMouseUp_: function(e) {
      if (this.dragging_) {
        // Make sure the dragging location is connected.
        var primaryDiv = this.displays_[0].div;
        var draggingDiv = this.dragging_.display.div;
        if (this.layout_ == SecondaryDisplayLayout.LEFT ||
            this.layout_ == SecondaryDisplayLayout.RIGHT) {
          var top = Math.max(draggingDiv.offsetTop,
                             primaryDiv.offsetTop - draggingDiv.offsetHeight +
                             MIN_OFFSET_OVERLAP);
          top = Math.min(top,
                         primaryDiv.offsetTop + primaryDiv.offsetHeight -
                         MIN_OFFSET_OVERLAP);
          draggingDiv.style.top = top + 'px';
        } else {
          var left = Math.max(draggingDiv.offsetLeft,
                              primaryDiv.offsetLeft - draggingDiv.offsetWidth +
                              MIN_OFFSET_OVERLAP);
          left = Math.min(left,
                          primaryDiv.offsetLeft + primaryDiv.offsetWidth -
                          MIN_OFFSET_OVERLAP);
          draggingDiv.style.left = left + 'px';
        }
        this.dragging_ = null;
        if (this.dirty_)
          this.applyResult_();
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

      var arrow = $('display-configuration-arrow');
      arrow.hidden = false;
      arrow.style.top =
          $('display-configurations').offsetTop - arrow.offsetHeight / 2 + 'px';
      arrow.style.left = display.div.offsetLeft + display.div.offsetWidth / 2 -
          arrow.offsetWidth / 2 + 'px';
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
      this.displaysView_.onmousemove = this.onMouseMove_.bind(this);
      this.displaysView_.onmousedown = this.onMouseDown_.bind(this);
      this.displaysView_.onmouseup = this.onMouseUp_.bind(this);
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
      /** @const */ var FOCUSED_BORDER_WIDTH_PX = 2;
      /** @const */ var NORMAL_BORDER_WIDTH_PX = 1;
      var borderWidth = (index == this.focusedIndex_) ?
          FOCUSED_BORDER_WIDTH_PX : NORMAL_BORDER_WIDTH_PX;
      display.div.style.width =
          display.width * this.visualScale_ - borderWidth * 2 + 'px';
      display.div.style.height =
          display.height * this.visualScale_ - borderWidth * 2 + 'px';
      display.div.style.lineHeight = display.div.style.height;
      if (index == 0) {
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
      // Offset pixels for secondary display rectangles.
      /** @const */ var MIRRORING_OFFSET_PIXELS = 2;
      // Always show two displays because there must be two displays when
      // the display_options is enabled.  Don't rely on displays_.length because
      // there is only one display from chrome's perspective in mirror mode.
      /** @const */ var MIN_NUM_DISPLAYS = 2;
      /** @const */ var MIRRORING_VERTICAL_MARGIN = 20;

      // The width/height should be same as the primary display:
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

        if (i == 0) {
          // Assumes that first display is primary and put a grey rectangle to
          // denote launcher below.
          var launcher = document.createElement('div');
          launcher.id = 'display-launcher';
          div.appendChild(launcher);
        }
        this.resizeDisplayRectangle_(display, i);
        div.style.left = display.x * this.visualScale_ + offset.x + 'px';
        div.style.top = display.y * this.visualScale_ + offset.y + 'px';
        div.appendChild(document.createTextNode(display.name));

        this.displaysView_.appendChild(div);
      }
    },

    /**
     * Called when the display arrangement has changed.
     * @private
     * @param {boolean} mirroring Whether current mode is mirroring or not.
     * @param {Array} displays The list of the display information.
     * @param {SecondaryDisplayLayout} layout The layout strategy.
     * @param {number} offset The offset of the secondary display.
     */
    onDisplayChanged_: function(mirroring, displays, layout, offset) {
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
      this.updateSelectedDisplayDescription_();
    },
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
