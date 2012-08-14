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
   * values in ash/monitor/monitor_controller.cc.
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
                     'display-options');
    this.mirroring_ = false;
    this.focusedIndex_ = null;
    this.displays_ = [];
  }

  cr.addSingletonGetter(DisplayOptions);

  DisplayOptions.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('display-options-toggle-mirroring').onclick = (function() {
        this.mirroring_ = !this.mirroring_;
        chrome.send('setMirroring', [this.mirroring_]);
      }).bind(this);

      $('display-options-apply').onclick = this.applyResult_.bind(this);
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
      chrome.send('setDisplayLayout', [this.layout_, offset / VISUAL_SCALE]);
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

      var primaryDiv = this.displays_[0].div;
      var display = this.dragging_.display;

      // Separate the area into four (LEFT/RIGHT/TOP/BOTTOM) by the diagonals of
      // the primary display, and decide which area the display should reside.
      var diagonalSlope = primaryDiv.offsetHeight / primaryDiv.offsetWidth;
      var topDownIntercept =
          primaryDiv.offsetTop - primaryDiv.offsetLeft * diagonalSlope;
      var bottomUpIntercept = primaryDiv.offsetTop +
          primaryDiv.offsetHeight + primaryDiv.offsetLeft * diagonalSlope;

      if (mousePosition.y >
          topDownIntercept + mousePosition.x * diagonalSlope) {
        if (mousePosition.y >
            bottomUpIntercept - mousePosition.x * diagonalSlope)
          this.layout_ = SecondaryDisplayLayout.BOTTOM;
        else
          this.layout_ = SecondaryDisplayLayout.LEFT;
      } else {
        if (mousePosition.y >
            bottomUpIntercept - mousePosition.x * diagonalSlope)
          this.layout_ = SecondaryDisplayLayout.RIGHT;
        else
          this.layout_ = SecondaryDisplayLayout.TOP;
      }

      if (this.layout_ == SecondaryDisplayLayout.LEFT ||
          this.layout_ == SecondaryDisplayLayout.RIGHT) {
        if (newPosition.y > primaryDiv.offsetTop + primaryDiv.offsetHeight)
          this.layout_ = SecondaryDisplayLayout.BOTTOM;
        else if (newPosition.y + display.div.offsetHeight <
                 primaryDiv.offsetTop)
          this.layout_ = SecondaryDisplayLayout.TOP;
      } else {
        if (newPosition.y > primaryDiv.offsetLeft + primaryDiv.offsetWidth)
          this.layout_ = SecondaryDisplayLayout.RIGHT;
        else if (newPosition.y + display.div.offsetWidth <
                   primaryDiv.offstLeft)
          this.layout_ = SecondaryDisplayLayout.LEFT;
      }

      switch (this.layout_) {
      case SecondaryDisplayLayout.RIGHT:
        display.div.style.left =
            primaryDiv.offsetLeft + primaryDiv.offsetWidth + 'px';
        display.div.style.top = newPosition.y + 'px';
        break;
      case SecondaryDisplayLayout.LEFT:
        display.div.style.left =
            primaryDiv.offsetLeft - display.div.offsetWidth + 'px';
        display.div.style.top = newPosition.y + 'px';
        break;
      case SecondaryDisplayLayout.TOP:
        display.div.style.top =
            primaryDiv.offsetTop - display.div.offsetHeight + 'px';
        display.div.style.left = newPosition.x + 'px';
        break;
      case SecondaryDisplayLayout.BOTTOM:
        display.div.style.top =
            primaryDiv.offsetTop + primaryDiv.offsetHeight + 'px';
        display.div.style.left = newPosition.x + 'px';
        break;
      }

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
        if (i != this.focusedIndex_)
          continue;

        display.div.classList.add('displays-focused');
        // Do not drag the primary monitor.
        if (i == 0)
          continue;

        this.dragging_ = {
            display: display,
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
      var width = this.displays_[0].width * VISUAL_SCALE;
      var height = this.displays_[0].height * VISUAL_SCALE;

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
      var totalHeight = 0;
      var boundingBox = {left: 0, right: 0, top: 0, bottom: 0};
      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        totalHeight += display.height * VISUAL_SCALE;
        boundingBox.left = Math.min(boundingBox.left, display.x * VISUAL_SCALE);
        boundingBox.right = Math.max(
            boundingBox.right, (display.x + display.width) * VISUAL_SCALE);
        boundingBox.top = Math.min(boundingBox.top, display.y * VISUAL_SCALE);
        boundingBox.bottom = Math.max(
            boundingBox.bottom, (display.y + display.height) * VISUAL_SCALE);
      }

      // Prepare enough area for thisplays_view by adding the maximum height.
      this.displaysView_.style.height = totalHeight + 'px';

      // Centering the bounding box of the display rectangles.
      var offset = {x: $('display-options-displays-view').offsetWidth / 2 -
                       (boundingBox.left + boundingBox.right) / 2,
                    y: totalHeight / 2 -
                       (boundingBox.top + boundingBox.bottom) / 2};


      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        var div = document.createElement('div');
        display.div = div;

        div.className = 'displays-display';
        if (i == this.focusedIndex_)
          div.classList.add('displays-focused');
        div.style.width = display.width * VISUAL_SCALE + 'px';
        div.style.height = display.height * VISUAL_SCALE + 'px';
        div.style.lineHeight = div.style.height;
        if (i == 0) {
          // Assumes that first display is primary and put a grey rectangle to
          // denote launcher below.
          var launcher = document.createElement('div');
          launcher.id = 'display-launcher';
          launcher.style.width = display.div.style.width;
          div.appendChild(launcher);
        }
        div.style.left = display.x * VISUAL_SCALE + offset.x + 'px';
        div.style.top = display.y * VISUAL_SCALE + offset.y + 'px';

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
