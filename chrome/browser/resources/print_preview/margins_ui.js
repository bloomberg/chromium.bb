// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'strict';

  function MarginsUI() {
    var marginsUI = document.createElement('div');
    marginsUI.__proto__ = MarginsUI.prototype;
    marginsUI.id = 'customized-margins';

    // @type {print_preview.MarginsUIPair} The object corresponding to the top
    //     margin.
    marginsUI.topPair_ = new print_preview.MarginsUIPair(
        print_preview.MarginSettings.TOP_GROUP);
    // @type {print_preview.MarginsUIPair} The object corresponding to the left
    //     margin.
    marginsUI.leftPair_ = new print_preview.MarginsUIPair(
        print_preview.MarginSettings.LEFT_GROUP);
    // @type {print_preview.MarginsUIPair} The object corresponding to the right
    //     margin.
    marginsUI.rightPair_ = new print_preview.MarginsUIPair(
        print_preview.MarginSettings.RIGHT_GROUP);
    // @type {print_preview.MarginsUIPair} The object corresponding to the
    //     bottom margin.
    marginsUI.bottomPair_ = new print_preview.MarginsUIPair(
        print_preview.MarginSettings.BOTTOM_GROUP);

    var uiPairs = marginsUI.pairsAsList;
    for (var i = 0; i < uiPairs.length; i++)
      marginsUI.appendChild(uiPairs[i]);

    // @type {print_preview.MarginsUIPair} The object that is being dragged.
    //     null if no drag session is in progress.
    marginsUI.lastClickedMarginsUIPair = null;

    // @type {EventTracker} Used to keep track of certain event listeners.
    marginsUI.eventTracker_ = new EventTracker();

    marginsUI.addEventListeners_();
    return marginsUI;
  }

  /**
   * @param {{x: number, y: number}} point The point with respect to the top
   *     left corner of the webpage.
   * @return {{x: number: y: number}} The point with respect to the top left
   *     corner of the plugin area.
   */
  MarginsUI.convert = function(point) {
    var mainview = $('mainview');
    return { x: point.x - mainview.offsetLeft,
             y: point.y - mainview.offsetTop };
  }

  MarginsUI.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Adds an observer for |customEvents.MARGINS_MAY_HAVE_CHANGED| event.
     * @param {function} func A callback function to be called when
     *     |customEvents.MARGINS_MAY_HAVE_CHANGED| event occurs.
     */
    addObserver: function(func) {
      var uiPairs = this.pairsAsList;
      for (var i = 0; i < uiPairs.length; i++) {
        uiPairs[i].box_.addEventListener(
            customEvents.MARGINS_MAY_HAVE_CHANGED, func);
      }
    },

    /**
     * @return {array} An array including all |MarginUIPair| objects.
     */
    get pairsAsList() {
      return [this.topPair_, this.leftPair_, this.rightPair_, this.bottomPair_];
    },

    /**
     * Updates the state of the margins UI.
     * @param {print_preview.Rect} marginsRectangle
     * @param {Margins} marginValues
     * @param {array} valueLimits
     */
    update: function(marginsRectangle, marginValues, valueLimits,
        keepDisplayedValue, valueLimitsInPercent) {
      var uiPairs = this.pairsAsList;
      var order = ['top', 'left', 'right', 'bottom'];
      for (var i = 0; i < uiPairs.length; i++) {
        uiPairs[i].update(marginsRectangle,
                          marginValues[order[i]],
                          valueLimits[i],
                          keepDisplayedValue,
                          valueLimitsInPercent[i]);
      }
    },

    /**
     * Draws |this| based on the latest state.
     */
    draw: function() {
      this.applyClippingMask_();
      this.pairsAsList.forEach(function(pair) { pair.draw(); });
    },

    /**
     * Shows the margins UI.
     */
    show: function() {
      this.hidden = false;
      this.classList.remove('invisible');
    },

    /**
     * Hides the margins UI and removes from the rendering flow if requested.
     * @param {boolean} removeFromFlow True if |this| should also be removed
     *     from the rendering flow (in order to not interfere with the tab
     *     order).
     */
    hide: function(removeFromFlow) {
      removeFromFlow ? this.hidden = true : this.classList.add('invisible');
    },

    /**
     * Applies a clipping mask on |this| so that it does not paint on top of the
     * scrollbars (if any).
     */
    applyClippingMask_: function() {
      var bottom = previewArea.height;
      var right = previewArea.width;
      this.style.clip = 'rect(0, ' + right + 'px, ' + bottom + 'px, 0)';
    },

    /**
     * Adds event listeners for various events.
     * @private
     */
    addEventListeners_: function() {
      var uiPairs = this.pairsAsList;
      for (var i = 0; i < uiPairs.length; i++) {
        uiPairs[i].addEventListener(customEvents.MARGIN_LINE_MOUSE_DOWN,
                                    this.onMarginLineMouseDown.bind(this));
      }
      // After snapping to min/max the MarginUIPair might not receive the
      // mouseup event since it is not under the mouse pointer, so it is handled
      // here.
      window.document.addEventListener('mouseup',
                                       this.onMarginLineMouseUp.bind(this));
    },

    /**
     * Executes when a "MarginLineMouseDown" event occurs.
     * @param {cr.Event} e The event that triggered this listener.
     */
    onMarginLineMouseDown: function(e) {
      this.lastClickedMarginsUIPair = e.target;
      this.bringToFront(this.lastClickedMarginsUIPair);
      // Note: Capturing mouse events at a higher level in the DOM than |this|,
      // so that the plugin can still receive mouse events.
      this.eventTracker_.add(
          window.document, 'mousemove', this.onMouseMove_.bind(this), false);
    },

    /**
     * Executes when a "MarginLineMouseUp" event occurs.
     * @param {cr.Event} e The event that triggered this listener.
     */
    onMarginLineMouseUp: function(e) {
      if (this.lastClickedMarginsUIPair == null)
        return;
      this.lastClickedMarginsUIPair.onMouseUp();
      this.lastClickedMarginsUIPair = null;
      this.eventTracker_.remove(window.document, 'mousemove');
    },

    /**
     * Brings |uiPair| in front of the other pairs. Used to make sure that the
     * dragged pair is visible when overlapping with a not dragged pair.
     * @param {print_preview.MarginsUIPair} uiPair The pair to bring in front.
     */
    bringToFront: function(uiPair) {
      this.pairsAsList.forEach(function(pair) {
        pair.classList.remove('dragging');
      });
      uiPair.classList.add('dragging');
    },

    /**
     * Executes when a mousemove event occurs.
     * @param {MouseEvent} e The event that triggered this listener.
     */
    onMouseMove_: function(e) {
      var point = MarginsUI.convert({ x: e.x, y: e.y });

      var dragEvent = new cr.Event(customEvents.MARGIN_LINE_DRAG);
      dragEvent.dragDelta =
          this.lastClickedMarginsUIPair.getDragDisplacementFrom(point);
      dragEvent.destinationPoint = point;
      this.dispatchEvent(dragEvent);
    },
  };

  return {
    MarginsUI: MarginsUI
  };
});
