// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interactive visualizaiton of TimelineModel objects
 * based loosely on gantt charts. Each thread in the TimelineModel is given a
 * set of TimelineTracks, one per subrow in the thread. The Timeline class
 * acts as a controller, creating the individual tracks, while TimelineTracks
 * do actual drawing.
 *
 * Visually, the Timeline produces (prettier) visualizations like the following:
 *    Thread1:  AAAAAAAAAA         AAAAA
 *                  BBBB              BB
 *    Thread2:     CCCCCC                 CCCCC
 *
 */
cr.define('tracing', function() {

  /**
   * The TimelineViewport manages the transform used for navigating
   * within the timeline. It is a simple transform:
   *   x' = (x+pan) * scale
   *
   * The timeline code tries to avoid directly accessing this transform,
   * instead using this class to do conversion between world and view space,
   * as well as the math for centering the viewport in various interesting
   * ways.
   *
   * @constructor
   * @extends {cr.EventTarget}
   */
  function TimelineViewport() {
    this.scaleX_ = 1;
    this.panX_ = 0;
    this.gridTimebase_ = 0;
    this.gridStep_ = 1000 / 60;
    this.gridEnabled_ = false;
  }

  TimelineViewport.prototype = {
    __proto__: cr.EventTarget.prototype,

    get scaleX() {
      return this.scaleX_;
    },
    set scaleX(s) {
      var changed = this.scaleX_ != s;
      if (changed) {
        this.scaleX_ = s;
        cr.dispatchSimpleEvent(this, 'change');
      }
    },

    get panX() {
      return this.panX_;
    },
    set panX(p) {
      var changed = this.panX_ != p;
      if (changed) {
        this.panX_ = p;
        cr.dispatchSimpleEvent(this, 'change');
      }
    },

    setPanAndScale: function(p, s) {
      var changed = this.scaleX_ != s || this.panX_ != p;
      if (changed) {
        this.scaleX_ = s;
        this.panX_ = p;
        cr.dispatchSimpleEvent(this, 'change');
      }
    },

    xWorldToView: function(x) {
      return (x + this.panX_) * this.scaleX_;
    },

    xWorldVectorToView: function(x) {
      return x * this.scaleX_;
    },

    xViewToWorld: function(x) {
      return (x / this.scaleX_) - this.panX_;
    },

    xViewVectorToWorld: function(x) {
      return x / this.scaleX_;
    },

    xPanWorldPosToViewPos: function(worldX, viewX, viewWidth) {
      if (typeof viewX == 'string') {
        if (viewX == 'left') {
          viewX = 0;
        } else if (viewX == 'center') {
          viewX = viewWidth / 2;
        } else if (viewX == 'right') {
          viewX = viewWidth - 1;
        } else {
          throw Error('unrecognized string for viewPos. left|center|right');
        }
      }
      this.panX = (viewX / this.scaleX_) - worldX;
    },

    get gridEnabled() {
      return this.gridEnabled_;
    },

    set gridEnabled(enabled) {
      if (this.gridEnabled_ == enabled)
        return;
      this.gridEnabled_ = enabled && true;
      cr.dispatchSimpleEvent(this, 'change');
    },

    get gridTimebase() {
      return this.gridTimebase_;
    },

    set gridTimebase(timebase) {
      if (this.gridTimebase_ == timebase)
        return;
      this.gridTimebase_ = timebase;
      cr.dispatchSimpleEvent(this, 'change');
    },

    get gridStep() {
      return this.gridStep_;
    },

    applyTransformToCanavs: function(ctx) {
      ctx.transform(this.scaleX_, 0, 0, 1, this.panX_ * this.scaleX_, 0);
    }
  };

  /**
   * Renders a TimelineModel into a div element, making one
   * TimelineTrack for each subrow in each thread of the model, managing
   * overall track layout, and handling user interaction with the
   * viewport.
   *
   * @constructor
   * @extends {HTMLDivElement}
   */
  Timeline = cr.ui.define('div');

  Timeline.prototype = {
    __proto__: HTMLDivElement.prototype,

    model_: null,

    decorate: function() {
      this.classList.add('timeline');
      this.needsViewportReset_ = false;

      this.viewport_ = new TimelineViewport();
      this.viewport_.addEventListener('change',
                                      this.viewportChange_.bind(this));

      this.invalidatePending_ = false;

      this.tracks_ = this.ownerDocument.createElement('div');
      this.tracks_.invalidate = this.invalidate.bind(this);
      this.appendChild(this.tracks_);

      this.dragBox_ = this.ownerDocument.createElement('div');
      this.dragBox_.className = 'timeline-drag-box';
      this.appendChild(this.dragBox_);
      this.hideDragBox_();

      // The following code uses a setInterval to monitor the timeline control
      // for size changes. This is so that we can keep the canvas' bitmap size
      // correctly synchronized with its presentation size.
      // TODO(nduca): detect this in a more efficient way, e.g. iframe hack.
      this.lastSize_ = this.clientWidth + 'x' + this.clientHeight;
      this.checkForResizeInterval_ =
          this.ownerDocument.defaultView.setInterval(function() {
        if (!this.isAttachedToDocument_)
          return;
        var curSize = this.clientWidth + 'x' + this.clientHeight;
        if (this.clientWidth && curSize != this.lastSize_) {
          this.lastSize_ = curSize;
          this.onResize();
        }
      }.bind(this), 250);

      this.bindEventListener_(document, 'keypress', this.onKeypress_, this);
      this.bindEventListener_(document, 'keydown', this.onKeydown_, this);
      this.bindEventListener_(document, 'mousedown', this.onMouseDown_, this);
      this.bindEventListener_(document, 'mousemove', this.onMouseMove_, this);
      this.bindEventListener_(document, 'mouseup', this.onMouseUp_, this);
      this.bindEventListener_(document, 'dblclick', this.onDblClick_, this);

      this.lastMouseViewPos_ = {x: 0, y: 0};

      this.selection_ = [];
    },

    /**
     * Wraps the standard addEventListener but automatically binds the provided
     * func to the provided target, tracking the resulting closure. When detach
     * is called, these listeners will be automatically removed.
     */
    bindEventListener_: function(object, event, func, target) {
      if (!this.boundListeners_)
        this.boundListeners_ = [];
      var boundFunc = func.bind(target);
      this.boundListeners_.push({object: object,
        event: event,
        boundFunc: boundFunc});
      object.addEventListener(event, boundFunc);
    },

    detach: function() {
      for (var i = 0; i < this.boundListeners_.length; i++) {
        var binding = this.boundListeners_[i];
        binding.object.removeEventListener(binding.event, binding.boundFunc);
      }
      this.boundListeners_ = undefined;
      window.clearInterval(this.checkForResizeInterval_);
      this.checkForResizeInterval_ = undefined;
    },

    get model() {
      return this.model_;
    },

    set model(model) {
      if (!model)
        throw Error('Model cannot be null');
      if (this.model) {
        throw Error('Cannot set model twice.');
      }
      this.model_ = model;

      // Create tracks and measure their heading size.
      var threads = model.getAllThreads();
      var maxHeadingWidth = 0;
      var tracks = [];
      var measuringStick = new tracing.MeasuringStick();
      var headingEl = document.createElement('div');
      headingEl.style.position = 'fixed';
      headingEl.className = 'timeline-slice-track-title';
      for (var tI = 0; tI < threads.length; tI++) {
        var thread = threads[tI];
        var track = new TimelineThreadTrack();
        track.thread = thread;
        track.viewport = this.viewport_;
        tracks.push(track);
        headingEl.textContent = track.heading;
        var w = measuringStick.measure(headingEl).width;
        // Limit heading width to 300px.
        if (w > 300)
          w = 300;
        if (w > maxHeadingWidth)
          maxHeadingWidth = w;
      }
      var extraHeadingPadding = 4;
      maxHeadingWidth += maxHeadingWidth + extraHeadingPadding;

      // Attach tracks and set width.
      this.tracks_.textContent = '';
      threads.sort(tracing.TimelineThread.compare);
      for (var tI = 0; tI < tracks.length; tI++) {
        var track = tracks[tI];
        track.headingWidth = maxHeadingWidth + 'px';
        this.tracks_.appendChild(track);
      }

      if (this.isAttachedToDocument_)
        this.onResize();
      else
        this.needsViewportReset_ = true;
    },

    viewportChange_: function() {
      this.invalidate();
    },

    invalidate: function() {
      if (this.invalidatePending_)
        return;
      this.invalidatePending_ = true;
      if (this.isAttachedToDocument_)
        window.setTimeout(function() {
          this.invalidatePending_ = false;
          this.redrawAllTracks_();
        }.bind(this), 0);
    },

    /**
     * @return {boolean} Whether the current timeline is attached to the
     * document.
     */
    get isAttachedToDocument_() {
      var cur = this;
      while (cur.parentNode)
        cur = cur.parentNode;
      return cur == this.ownerDocument;
    },

    onResize: function() {
      if (!this.isAttachedToDocument_)
        throw 'Not attached to document!';
      for (var i = 0; i < this.tracks_.children.length; i++) {
        var track = this.tracks_.children[i];
        track.onResize();
      }
      if (this.invalidatePending_) {
        this.invalidatePending_ = false;
        this.redrawAllTracks_();
      }
    },

    redrawAllTracks_: function() {
      if (this.needsViewportReset_ && this.clientWidth != 0) {
        if (!this.isAttachedToDocument_)
          throw 'Not attached to document!';
        this.needsViewportReset_ = false;
        /* update viewport */
        var rangeTimestamp = this.model_.maxTimestamp -
            this.model_.minTimestamp;
        var w = this.firstCanvas.width;
        var scaleX = w / rangeTimestamp;
        var panX = -this.model_.minTimestamp;
        this.viewport_.setPanAndScale(panX, scaleX);
      }
      for (var i = 0; i < this.tracks_.children.length; i++) {
        this.tracks_.children[i].redraw();
      }
    },

    updateChildViewports_: function() {
      for (var cI = 0; cI < this.tracks_.children.length; cI++) {
        var child = this.tracks_.children[cI];
        child.setViewport(this.panX, this.scaleX);
      }
    },

    get listenToKeys_() {
      if (this.parentElement.parentElement.tabIndex >= 0)
        return document.activeElement == this.parentElement.parentElement;
      return true;
    },

    onKeypress_: function(e) {
      var vp = this.viewport_;
      if (!this.firstCanvas)
        return;
      if (!this.listenToKeys_)
        return;
      var viewWidth = this.firstCanvas.clientWidth;
      var curMouseV, curCenterW;
      switch (e.keyCode) {
        case 101: // e
          var vX = this.lastMouseViewPos_.x;
          var wX = vp.xViewToWorld(this.lastMouseViewPos_.x);
          var distFromCenter = vX - (viewWidth / 2);
          var percFromCenter = distFromCenter / viewWidth;
          var percFromCenterSq = percFromCenter * percFromCenter;
          vp.xPanWorldPosToViewPos(wX, 'center', viewWidth);
          break;
        case 119:  // w
          this.zoomBy_(1.5);
          break;
        case 115:  // s
          this.zoomBy_(1 / 1.5);
          break;
        case 103:  // g
          this.onGridToggle_(true);
          break;
        case 71:  // G
          this.onGridToggle_(false);
          break;
        case 87:  // W
          this.zoomBy_(10);
          break;
        case 83:  // S
          this.zoomBy_(1 / 10);
          break;
        case 97:  // a
          vp.panX += vp.xViewVectorToWorld(viewWidth * 0.1);
          break;
        case 100:  // d
          vp.panX -= vp.xViewVectorToWorld(viewWidth * 0.1);
          break;
        case 65:  // A
          vp.panX += vp.xViewVectorToWorld(viewWidth * 0.5);
          break;
        case 68:  // D
          vp.panX -= vp.xViewVectorToWorld(viewWidth * 0.5);
          break;
      }
    },

    // Not all keys send a keypress.
    onKeydown_: function(e) {
      if (!this.listenToKeys_)
        return;
      switch (e.keyCode) {
        case 37:   // left arrow
          this.selectPrevious_(e);
          e.preventDefault();
          break;
        case 39:   // right arrow
          this.selectNext_(e);
          e.preventDefault();
          break;
        case 9:    // TAB
          if (this.parentElement.parentElement.tabIndex == -1) {
            if (e.shiftKey)
              this.selectPrevious_(e);
            else
              this.selectNext_(e);
            e.preventDefault();
          }
          break;
      }
    },

    /**
     * Zoom in or out on the timeline by the given scale factor.
     * @param {integer} scale The scale factor to apply.  If <1, zooms out.
     */
    zoomBy_: function(scale) {
      if (!this.firstCanvas)
        return;
      var vp = this.viewport_;
      var viewWidth = this.firstCanvas.clientWidth;
      var curMouseV = this.lastMouseViewPos_.x;
      var curCenterW = vp.xViewToWorld(curMouseV);
      vp.scaleX = vp.scaleX * scale;
      vp.xPanWorldPosToViewPos(curCenterW, curMouseV, viewWidth);
    },

    /** Select the next slice on the timeline.  Applies to each track. */
    selectNext_: function(e) {
      this.selectAdjoining_(e, true);
    },

    /** Select the previous slice on the timeline.  Applies to each track. */
    selectPrevious_: function(e) {
      this.selectAdjoining_(e, false);
    },

    /**
     * Helper for selection previous or next.
     * @param {Event} The current event.
     * @param {boolean} forwardp If true, select one forward (next).
     *   Else, select previous.
     */
    selectAdjoining_: function(e, forwardp) {
      var i, track, slice, adjoining;
      var selection = [];
      // Clear old selection; try and select next.
      for (i = 0; i < this.selection_.length; i++) {
        adjoining = undefined;
        this.selection_[i].slice.selected = false;
        track = this.selection_[i].track;
        slice = this.selection_[i].slice;
        if (slice) {
          if (forwardp)
            adjoining = track.pickNext(slice);
          else
            adjoining = track.pickPrevious(slice);
        }
        if (adjoining != undefined)
          selection.push({track: track, slice: adjoining});
      }
      // Activate the new selection.
      this.selection_ = selection;
      for (i = 0; i < this.selection_.length; i++)
        this.selection_[i].slice.selected = true;
      cr.dispatchSimpleEvent(this, 'selectionChange');
      this.invalidate();  // Cause tracks to redraw.
      e.preventDefault();
    },

    get keyHelp() {
      var help = 'Keyboard shortcuts:\n' +
          ' w/s     : Zoom in/out    (with shift: go faster)\n' +
          ' a/d     : Pan left/right\n' +
          ' e       : Center on mouse\n' +
          ' g/G     : Shows grid at the start/end of the selected task\n';

      if (this.parentElement.parentElement.tabIndex) {
        help += ' <-      : Select previous event on current timeline\n' +
            ' ->      : Select next event on current timeline\n';
      } else {
        help += ' <-,^TAB : Select previous event on current timeline\n' +
            ' ->, TAB : Select next event on current timeline\n';
      }

      help +=
          '\n' +
          'Dbl-click to zoom in; Shift dbl-click to zoom out\n';
      return help;
    },

    get selection() {
      return this.selection_;
    },

    set selection(selection) {
      // Clear old selection.
      for (i = 0; i < this.selection_.length; i++)
        this.selection_[i].slice.selected = false;

      this.selection_ = selection;

      cr.dispatchSimpleEvent(this, 'selectionChange');
      for (i = 0; i < this.selection_.length; i++)
        this.selection_[i].slice.selected = true;
      this.invalidate();  // Cause tracks to redraw.
    },

    get firstCanvas() {
      return this.tracks_.firstChild ?
          this.tracks_.firstChild.firstCanvas : undefined;
    },

    hideDragBox_: function() {
      this.dragBox_.style.left = '-1000px';
      this.dragBox_.style.top = '-1000px';
      this.dragBox_.style.width = 0;
      this.dragBox_.style.height = 0;
    },

    setDragBoxPosition_: function(eDown, eCur) {
      var loX = Math.min(eDown.clientX, eCur.clientX);
      var hiX = Math.max(eDown.clientX, eCur.clientX);
      var loY = Math.min(eDown.clientY, eCur.clientY);
      var hiY = Math.max(eDown.clientY, eCur.clientY);

      this.dragBox_.style.left = loX + 'px';
      this.dragBox_.style.top = loY + 'px';
      this.dragBox_.style.width = hiX - loX + 'px';
      this.dragBox_.style.height = hiY - loY + 'px';

      var canv = this.firstCanvas;
      var loWX = this.viewport_.xViewToWorld(loX - canv.offsetLeft);
      var hiWX = this.viewport_.xViewToWorld(hiX - canv.offsetLeft);

      var roundedDuration = Math.round((hiWX - loWX) * 100) / 100;
      this.dragBox_.textContent = roundedDuration + 'ms';

      var e = new cr.Event('selectionChanging');
      e.loWX = loWX;
      e.hiWX = hiWX;
      this.dispatchEvent(e);
    },

    onGridToggle_: function(left) {
      var tb;
      if (left)
        tb = Math.min.apply(Math, this.selection_.map(
            function(x) { return x.slice.start; }));
      else
        tb = Math.max.apply(Math, this.selection_.map(
            function(x) { return x.slice.end; }));

      // Shift the timebase left until its just left of minTimestamp.
      var numInterfvalsSinceStart = Math.ceil((tb - this.model_.minTimestamp) /
          this.viewport_.gridStep_);
      this.viewport_.gridTimebase = tb -
          (numInterfvalsSinceStart + 1) * this.viewport_.gridStep_;
      this.viewport_.gridEnabled = true;
    },

    onMouseDown_: function(e) {
      if (e.clientX < this.offsetLeft ||
          e.clientX >= this.offsetLeft + this.offsetWidth ||
          e.clientY < this.offsetTop ||
          e.clientY >= this.offsetTop + this.offsetHeight)
        return;

      var canv = this.firstCanvas;
      var pos = {
        x: e.clientX - canv.offsetLeft,
        y: e.clientY - canv.offsetTop
      };

      var wX = this.viewport_.xViewToWorld(pos.x);

      this.dragBeginEvent_ = e;
      e.preventDefault();
      if (this.parentElement.parentElement.tabIndex)
        this.parentElement.parentElement.focus();
    },

    onMouseMove_: function(e) {
      if (!this.firstCanvas)
        return;
      var canv = this.firstCanvas;
      var pos = {
        x: e.clientX - canv.offsetLeft,
        y: e.clientY - canv.offsetTop
      };

      // Remember position. Used during keyboard zooming.
      this.lastMouseViewPos_ = pos;

      // Update the drag box
      if (this.dragBeginEvent_) {
        this.setDragBoxPosition_(this.dragBeginEvent_, e);
      }
    },

    onMouseUp_: function(e) {
      var i;
      if (this.dragBeginEvent_) {
        // Stop the dragging.
        this.hideDragBox_();
        var eDown = this.dragBeginEvent_;
        this.dragBeginEvent_ = null;

        // Figure out extents of the drag.
        var loX = Math.min(eDown.clientX, e.clientX);
        var hiX = Math.max(eDown.clientX, e.clientX);
        var loY = Math.min(eDown.clientY, e.clientY);
        var hiY = Math.max(eDown.clientY, e.clientY);

        // Convert to worldspace.
        var canv = this.firstCanvas;
        var loWX = this.viewport_.xViewToWorld(loX - canv.offsetLeft);
        var hiWX = this.viewport_.xViewToWorld(hiX - canv.offsetLeft);

        // Figure out what has been hit.
        var selection = [];
        function addHit(type, track, slice) {
          selection.push({track: track, slice: slice});
        }
        for (i = 0; i < this.tracks_.children.length; i++) {
          var track = this.tracks_.children[i];

          // Only check tracks that insersect the rect.
          var trackClientRect = track.getBoundingClientRect();
          var a = Math.max(loY, trackClientRect.top);
          var b = Math.min(hiY, trackClientRect.bottom);
          if (a <= b) {
            track.pickRange(loWX, hiWX, loY, hiY, addHit);
          }
        }
        // Activate the new selection.
        this.selection = selection;
      }
    },

    onDblClick_: function(e) {
      var scale = 4;
      if (e.shiftKey)
        scale = 1 / scale;
      this.zoomBy_(scale);
      e.preventDefault();
    },
  };

  /**
   * The TimelineModel being viewed by the timeline
   * @type {TimelineModel}
   */
  cr.defineProperty(Timeline, 'model', cr.PropertyKind.JS);

  return {
    Timeline: Timeline
  };
});
