// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('options');

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

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  // The scale ratio of the display rectangle to its original size.
  /** @const */ var VISUAL_SCALE = 1 / 10;

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
     * Manages the display layout.
     * @type {?options.DisplayLayoutManager}
     * @private
     */
    displayLayoutManager_: null,

    /**
     * The id of the currently focused display, or empty for none.
     * @type {string}
     * @private
     */
    focusedId_: '',

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
     * Sends the display layout for the secondary display to Chrome.
     * @private
     */
    sendDragResult_: function() {
      // The first non-primary display is the secondary display.
      var secondaryId;
      for (var i = 0; i < this.displays_.length; i++) {
        if (!this.displays_[i].isPrimary) {
          secondaryId = this.displays_[i].id;
          break;
        }
      }
      assert(!!secondaryId);
      var displayLayout =
          this.displayLayoutManager_.getDisplayLayout(secondaryId);
      chrome.send(
          'setDisplayLayout',
          [secondaryId, displayLayout.layoutType, displayLayout.offset]);
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

      var dragInfo = this.dragInfo_;

      /** @type {options.DisplayPosition} */ var newPosition = {
        x: dragInfo.originalLocation.x +
            (eventLocation.x - dragInfo.eventLocation.x),
        y: dragInfo.originalLocation.y +
            (eventLocation.y - dragInfo.eventLocation.y)
      };

      this.displayLayoutManager_.updatePosition(
          this.dragInfo_.displayId, newPosition);

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
        var displayLayout =
            this.displayLayoutManager_.getDisplayLayout(this.displays_[i].id);
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
        var displayLayout =
            this.displayLayoutManager_.getDisplayLayout(this.displays_[i].id);
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

      if (this.displayLayoutManager_.finalizePosition(this.dragInfo_.displayId))
        this.sendDragResult_();

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
      var displayLayout =
          this.displayLayoutManager_.getDisplayLayout(display.id);
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
     * @param {string} parentId
     * @return {!options.DisplayLayout}
     * @private
     */
    createDisplayLayout_: function(display, layoutType, parentId) {
      return {
        bounds: display.bounds,
        div: null,
        id: display.id,
        layoutType: layoutType,
        name: display.name,
        offset: 0,
        originalPosition: {x: 0, y: 0},
        parentId: parentId
      };
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
      var primaryDisplayId = '';
      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        if (display.isPrimary && primaryDisplayId == '')
          primaryDisplayId = display.id;

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
      if (primaryDisplayId == '')
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

      // Create the layout manager.
      this.displayLayoutManager_ =
          new options.DisplayLayoutManager(this.visualScale_);

      // Create the display layouts.
      for (var i = 0; i < this.displays_.length; i++) {
        var display = this.displays_[i];
        var parentId = display.isPrimary ? '' : primaryDisplayId;
        var layout = this.createDisplayLayout_(display, layoutType, parentId);
        this.displayLayoutManager_.addDisplayLayout(layout);
      }

      // Create the display divs.
      this.displayLayoutManager_.createDisplayLayoutDivs(
          this.displaysView_, offset);

      // Set the div callbacks and highlight the focused div.
      for (var i = 0; i < this.displays_.length; i++) {
        var id = this.displays_[i].id;
        var div = this.displayLayoutManager_.getDisplayLayout(id).div;
        div.onmousedown = this.onMouseDown_.bind(this);
        div.ontouchstart = this.onTouchStart_.bind(this);
        div.classList.toggle('displays-focused', id == this.focusedId_);
      }
    },

    /**
     * Called when the display arrangement has changed.
     * @param {options.MultiDisplayMode} mode multi display mode.
     * @param {!Array<!options.DisplayInfo>} displays The list of the display
     *     information.
     * @param {options.DisplayLayoutType} layoutType The layout type for the
     *     secondary display.
     * @param {number} offset The offset of the secondary display.
     * @private
     */
    onDisplayChanged_: function(mode, displays, layoutType, offset) {
      if (!this.visible)
        return;

      this.displays_ = displays;

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
