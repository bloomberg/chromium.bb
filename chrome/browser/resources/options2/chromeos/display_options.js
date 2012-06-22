// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  // The scale ratio of the display rectangle to its original size.
  /** @const */ var VISUAL_SCALE = 1 / 10;

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
    this.focused_index_ = null;
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

      $('display-options-confirm').onclick = function() {
        OptionsPage.closeOverlay();
      };

      $('display-options-toggle-mirroring').onclick = (function() {
        this.mirroring_ = !this.mirroring_;
        chrome.send('setMirroring', [this.mirroring_]);
      }).bind(this);

      chrome.send('getDisplayInfo');
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
      var mouse_position = {
        x: e.pageX - this.dragging_.offset.x,
        y: e.pageY - this.dragging_.offset.y
      };
      var new_position = {
        x: mouse_position.x - this.dragging_.click_location.x,
        y: mouse_position.y - this.dragging_.click_location.y
      };

      var primary_div = this.displays_[0].div;
      var display = this.dragging_.display;

      // Separate the area into four (LEFT/RIGHT/TOP/BOTTOM) by the diagonals of
      // the primary display, and decide which area the display should reside.
      var diagonal_slope = primary_div.offsetHeight / primary_div.offsetWidth;
      var top_down_intercept =
          primary_div.offsetTop - primary_div.offsetLeft * diagonal_slope;
      var bottom_up_intercept = primary_div.offsetTop +
          primary_div.offsetHeight + primary_div.offsetLeft * diagonal_slope;

      if (mouse_position.y >
          top_down_intercept + mouse_position.x * diagonal_slope) {
        if (mouse_position.y >
            bottom_up_intercept - mouse_position.x * diagonal_slope)
          this.layout_ = SecondaryDisplayLayout.BOTTOM;
        else
          this.layout_ = SecondaryDisplayLayout.LEFT;
      } else {
        if (mouse_position.y >
            bottom_up_intercept - mouse_position.x * diagonal_slope)
          this.layout_ = SecondaryDisplayLayout.RIGHT;
        else
          this.layout_ = SecondaryDisplayLayout.TOP;
      }

      if (this.layout_ == SecondaryDisplayLayout.LEFT ||
          this.layout_ == SecondaryDisplayLayout.RIGHT) {
        if (new_position.y > primary_div.offsetTop + primary_div.offsetHeight)
          this.layout_ = SecondaryDisplayLayout.BOTTOM;
        else if (new_position.y + display.div.offsetHeight <
                 primary_div.offsetTop)
          this.layout_ = SecondaryDisplayLayout.TOP;
      } else {
        if (new_position.y > primary_div.offsetLeft + primary_div.offsetWidth)
          this.layout_ = SecondaryDisplayLayout.RIGHT;
        else if (new_position.y + display.div.offsetWidth <
                   primary_div.offstLeft)
          this.layout_ = SecondaryDisplayLayout.LEFT;
      }

      switch (this.layout_) {
      case SecondaryDisplayLayout.RIGHT:
        display.div.style.left =
            primary_div.offsetLeft + primary_div.offsetWidth + 'px';
        display.div.style.top = new_position.y + 'px';
        break;
      case SecondaryDisplayLayout.LEFT:
        display.div.style.left =
            primary_div.offsetLeft - display.div.offsetWidth + 'px';
        display.div.style.top = new_position.y + 'px';
        break;
      case SecondaryDisplayLayout.TOP:
        display.div.style.top =
            primary_div.offsetTop - display.div.offsetHeight + 'px';
        display.div.style.left = new_position.x + 'px';
        break;
      case SecondaryDisplayLayout.BOTTOM:
        display.div.style.top =
            primary_div.offsetTop + primary_div.offsetHeight + 'px';
        display.div.style.left = new_position.x + 'px';
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

      if (e.target == this.displays_view_)
        return true;

      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        if (display.div == e.target) {
          // Do not drag the primary monitor.
          if (i == 0)
            return true;

          this.focused_index_ = i;
          this.dragging_ = {
            display: display,
            click_location: {x: e.offsetX, y: e.offsetY},
            offset: {x: e.pageX - e.offsetX - display.div.offsetLeft,
                     y: e.pageY - e.offsetY - display.div.offsetTop}
          };
          if (display.div.className.indexOf('displays-focused') == -1)
            display.div.className += ' displays-focused';
        } else if (display.div.className.indexOf('displays-focused') >= 0) {
          // We can assume that '-primary' monitor doesn't have '-focused'.
          this.displays_[i].div.className = 'displays-display';
        }
      }
      return false;
    },

    /**
     * Mouse up handler for dragging display rectangle.
     * @private
     * @param {Event} e The mouse up event.
     */
    onMouseUp_: function(e) {
      if (this.dragging_) {
        this.dragging_ = null;
        chrome.send('setDisplayLayout', [this.layout_]);
      }
      return false;
    },

    /**
     * Clears the drawing area for display rectangles.
     * @private
     */
    resetDisplaysView_: function() {
      var displays_view_host = $('display-options-displays-view-host');
      displays_view_host.removeChild(displays_view_host.firstChild);
      this.displays_view_ = document.createElement('div');
      this.displays_view_.id = 'display-options-displays-view';
      this.displays_view_.onmousemove = this.onMouseMove_.bind(this);
      this.displays_view_.onmousedown = this.onMouseDown_.bind(this);
      this.displays_view_.onmouseup = this.onMouseUp_.bind(this);
      displays_view_host.appendChild(this.displays_view_);
    },

    /**
     * Lays out the display rectangles for mirroring.
     * @private
     */
    layoutMirroringDisplays_: function() {
      // The width/height should be same as the primary display:
      var width = this.displays_[0].width * VISUAL_SCALE;
      var height = this.displays_[0].height * VISUAL_SCALE;

      this.displays_view_.style.height =
        height + this.displays_.length * 2 + 'px';

      for (var i = 0; i < this.displays_.length; i++) {
        var div = document.createElement('div');
        this.displays_[i].div = div;
        div.className = 'displays-display';
        div.style.top = i * 2 + 'px';
        div.style.left = i * 2 + 'px';
        div.style.width = width + 'px';
        div.style.height = height + 'px';
        div.style.zIndex = i;
        if (i == this.displays_.length - 1)
          div.className += ' displays-primary';
        this.displays_view_.appendChild(div);
      }
    },

    /**
     * Layouts the display rectangles according to the current layout_.
     * @private
     */
    layoutDisplays_: function() {
      var total_size = {width: 0, height: 0};
      for (var i = 0; i < this.displays_.length; i++) {
        total_size.width += this.displays_[i].width * VISUAL_SCALE;
        total_size.height += this.displays_[i].height * VISUAL_SCALE;
      }

      this.displays_view_.style.width = total_size.width + 'px';
      this.displays_view_.style.height = total_size.height + 'px';

      var base_point = {x: 0, y: 0};
      if (this.layout_ == SecondaryDisplayLayout.LEFT)
        base_point.x = total_size.width;
      else if (this.layout_ == SecondaryDisplayLayout.TOP)
        base_point.y = total_size.height;

      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        var div = document.createElement('div');
        display.div = div;

        div.className = 'displays-display';
        if (i == 0)
          div.className += ' displays-primary';
        else if (i == this.focused_index_)
          div.className += ' displays-focused';
        div.style.width = display.width * VISUAL_SCALE + 'px';
        div.style.height = display.height * VISUAL_SCALE + 'px';
        div.style.lineHeight = div.style.height;
        switch (this.layout_) {
        case SecondaryDisplayLayout.RIGHT:
          display.div.style.top = '0';
          display.div.style.left = base_point.x + 'px';
          base_point.x += display.width * VISUAL_SCALE;
          break;
        case SecondaryDisplayLayout.LEFT:
          display.div.style.top = '0';
          base_point.x -= display.width * VISUAL_SCALE;
          display.div.style.left = base_point.x + 'px';
          break;
        case SecondaryDisplayLayout.TOP:
          display.div.style.left = '0';
          base_point.y -= display.height * VISUAL_SCALE;
          display.div.style.top = base_point.y + 'px';
          break;
        case SecondaryDisplayLayout.BOTTOM:
          display.div.style.left = '0';
          display.div.style.top = base_point.y + 'px';
          base_point.y += display.height * VISUAL_SCALE;
          break;
        }

        this.displays_view_.appendChild(div);
      }
    },

    /**
     * Called when the display arrangement has changed.
     * @private
     * @param {boolean} mirroring Whether current mode is mirroring or not.
     * @param {Array} displays The list of the display information.
     * @param {SecondaryDisplayLayout} layout The layout strategy.
     */
    onDisplayChanged_: function(mirroring, displays, layout) {
      this.mirroring_ = mirroring;
      this.layout_ = layout;

      $('display-options-toggle-mirroring').textContent =
          loadTimeData.getString(
              this.mirroring_ ? 'stopMirroring' : 'startMirroring');

      // Focus to the first display next to the primary one when |displays| list
      // is updated.
      if (this.displays_.length != displays.length)
        this.focused_index_ = 1;

      this.displays_ = displays;

      if (this.displays_.length <= 1)
        return;

      this.resetDisplaysView_();
      if (this.mirroring_)
        this.layoutMirroringDisplays_();
      else
        this.layoutDisplays_();
    },
  };

  DisplayOptions.setDisplayInfo = function(mirroring, displays, layout) {
    DisplayOptions.getInstance().onDisplayChanged_(mirroring, displays, layout);
  };

  // Export
  return {
    DisplayOptions: DisplayOptions
  };
});
