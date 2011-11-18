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
  function TimelineViewport(parentEl) {
    this.parentEl_ = parentEl;
    this.scaleX_ = 1;
    this.panX_ = 0;
    this.gridTimebase_ = 0;
    this.gridStep_ = 1000 / 60;
    this.gridEnabled_ = false;
    this.hasCalledSetupFunction_ = false;

    this.onResizeBoundToThis_ = this.onResize_.bind(this);

    // The following code uses an interval to detect when the parent element
    // is attached to the document. That is a trigger to run the setup function
    // and install a resize listener.
    this.checkForAttachInterval_ = setInterval(
        this.checkForAttach_.bind(this), 250);
  }

  TimelineViewport.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * Allows initialization of the viewport when the viewport's parent element
     * has been attached to the document and given a size.
     * @param {Function} fn Function to call when the viewport can be safely
     * initialized.
     */
    setWhenPossible: function(fn) {
      this.pendingSetFunction_ = fn;
    },

    /**
     * @return {boolean} Whether the current timeline is attached to the
     * document.
     */
    get isAttachedToDocument_() {
      var cur = this.parentEl_;
      while (cur.parentNode)
        cur = cur.parentNode;
      return cur == this.parentEl_.ownerDocument;
    },

    onResize_: function() {
      this.dispatchChangeEvent();
    },

    /**
     * Checks whether the parentNode is attached to the document.
     * When it is, it installs the iframe-based resize detection hook
     * and then runs the pendingSetFunction_, if present.
     */
    checkForAttach_: function() {
      if (!this.isAttachedToDocument_ || this.clientWidth == 0)
        return;

      if (!this.iframe_) {
        this.iframe_ = document.createElement('iframe');
        this.iframe_.style.cssText =
            'position:absolute;width:100%;height:0;border:0;visibility:hidden;';
        this.parentEl_.appendChild(this.iframe_);

        this.iframe_.contentWindow.addEventListener('resize',
                                                    this.onResizeBoundToThis_);
      }

      var curSize = this.clientWidth + 'x' + this.clientHeight;
      if (this.pendingSetFunction_) {
        this.lastSize_ = curSize;
        this.pendingSetFunction_();
        this.pendingSetFunction_ = undefined;
      }

      window.clearInterval(this.checkForAttachInterval_);
      this.checkForAttachInterval_ = undefined;
    },

    /**
     * Fires the change event on this viewport. Used to notify listeners
     * to redraw when the underlying model has been mutated.
     */
    dispatchChangeEvent: function() {
      cr.dispatchSimpleEvent(this, 'change');
    },

    detach: function() {
      if (this.checkForAttachInterval_) {
        window.clearInterval(this.checkForAttachInterval_);
        this.checkForAttachInterval_ = undefined;
      }
      this.iframe_.removeListener('resize', this.onResizeBoundToThis_);
      this.parentEl_.removeChild(this.iframe_);
    },

    get scaleX() {
      return this.scaleX_;
    },
    set scaleX(s) {
      var changed = this.scaleX_ != s;
      if (changed) {
        this.scaleX_ = s;
        this.dispatchChangeEvent();
      }
    },

    get panX() {
      return this.panX_;
    },
    set panX(p) {
      var changed = this.panX_ != p;
      if (changed) {
        this.panX_ = p;
        this.dispatchChangeEvent();
      }
    },

    setPanAndScale: function(p, s) {
      var changed = this.scaleX_ != s || this.panX_ != p;
      if (changed) {
        this.scaleX_ = s;
        this.panX_ = p;
        this.dispatchChangeEvent();
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
      this.dispatchChangeEvent();
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

      this.viewport_ = new TimelineViewport(this);

      this.tracks_ = this.ownerDocument.createElement('div');
      this.appendChild(this.tracks_);

      this.dragBox_ = this.ownerDocument.createElement('div');
      this.dragBox_.className = 'timeline-drag-box';
      this.appendChild(this.dragBox_);
      this.hideDragBox_();

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
      for (var i = 0; i < this.tracks_.children.length; i++)
        this.tracks_.children[i].detach();

      for (var i = 0; i < this.boundListeners_.length; i++) {
        var binding = this.boundListeners_[i];
        binding.object.removeEventListener(binding.event, binding.boundFunc);
      }
      this.boundListeners_ = undefined;
      this.viewport_.detach();
    },

    get viewport() {
      return this.viewport_;
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

      // Figure out all the headings.
      var allHeadings = [];
      model.getAllThreads().forEach(function(t) {
        allHeadings.push(t.userFriendlyName);
      });
      model.getAllCounters().forEach(function(c) {
        allHeadings.push(c.name);
      });

      // Figure out the maximum heading size.
      var maxHeadingWidth = 0;
      var measuringStick = new tracing.MeasuringStick();
      var headingEl = document.createElement('div');
      headingEl.style.position = 'fixed';
      headingEl.className = 'timeline-canvas-based-track-title';
      allHeadings.forEach(function(text) {
        headingEl.textContent = text + ':__';
        var w = measuringStick.measure(headingEl).width;
        // Limit heading width to 300px.
        if (w > 300)
          w = 300;
        if (w > maxHeadingWidth)
          maxHeadingWidth = w;
      });
      maxHeadingWidth = maxHeadingWidth + 'px';

      // Reset old tracks.
      for (var i = 0; i < this.tracks_.children.length; i++)
        this.tracks_.children[i].detach();
      this.tracks_.textContent = '';

      // Get a sorted list of processes.
      var processes = [];
      for (var pid in model.processes)
        processes.push(model.processes[pid]);
      processes.sort(tracing.TimelineProcess.compare);

      // Create tracks for each process.
      processes.forEach(function(process) {
        // Add counter tracks for this process.
        var counters = [];
        for (var tid in process.counters)
          counters.push(process.counters[tid]);
        counters.sort(tracing.TimelineCounter.compare);

        // Create the counters for this process.
        counters.forEach(function(counter) {
          var track = new tracing.TimelineCounterTrack();
          track.heading = counter.name + ':';
          track.headingWidth = maxHeadingWidth;
          track.viewport = this.viewport_;
          track.counter = counter;
          this.tracks_.appendChild(track);
        }.bind(this));

        // Get a sorted list of threads.
        var threads = [];
        for (var tid in process.threads)
          threads.push(process.threads[tid]);
        threads.sort(tracing.TimelineThread.compare);

        // Create the threads.
        threads.forEach(function(thread) {
          var track = new tracing.TimelineThreadTrack();
          track.heading = thread.userFriendlyName + ':';
          track.tooltip = thread.userFriendlyDetials;
          track.headingWidth = maxHeadingWidth;
          track.viewport = this.viewport_;
          track.thread = thread;
          this.tracks_.appendChild(track);
        }.bind(this));
      }.bind(this));

      // Set up a reasonable viewport.
      this.viewport_.setWhenPossible(function() {
        var rangeTimestamp = this.model_.maxTimestamp -
            this.model_.minTimestamp;
        var w = this.firstCanvas.width;
        var scaleX = w / rangeTimestamp;
        var panX = -this.model_.minTimestamp;
        this.viewport_.setPanAndScale(panX, scaleX);
      }.bind(this));
    },

    /**
     * @return {Element} The element whose focused state determines
     * whether to respond to keyboard inputs.
     * Defaults to the parent element.
     */
    get focusElement() {
      if (this.focusElement_)
        return this.focusElement_;
      return this.parentElement;
    },

    /**
     * Sets the element whose focus state will determine whether
     * to respond to keybaord input.
     */
    set focusElement(value) {
      this.focusElement_ = value;
    },

    get listenToKeys_() {
      if (!this.focusElement_)
        return true;
      if (this.focusElement.tabIndex >= 0)
        return document.activeElement == this.focusElement;
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
          if (this.focusElement.tabIndex == -1) {
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
      this.selection = selection;
      e.preventDefault();
    },

    get keyHelp() {
      var help = 'Keyboard shortcuts:\n' +
          ' w/s     : Zoom in/out    (with shift: go faster)\n' +
          ' a/d     : Pan left/right\n' +
          ' e       : Center on mouse\n' +
          ' g/G     : Shows grid at the start/end of the selected task\n';

      if (this.focusElement.tabIndex) {
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
      this.viewport_.dispatchChangeEvent(); // Triggers a redraw.
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
      rect = this.getClientRects()[0];
      if (!rect ||
          e.clientX < rect.left ||
          e.clientX >= rect.right ||
          e.clientY < rect.top ||
          e.clientY >= rect.bottom)
        return;

      var canv = this.firstCanvas;
      var pos = {
        x: e.clientX - canv.offsetLeft,
        y: e.clientY - canv.offsetTop
      };

      var wX = this.viewport_.xViewToWorld(pos.x);

      this.dragBeginEvent_ = e;
      e.preventDefault();
      if (this.focusElement.tabIndex >= 0)
        this.focusElement.focus();
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
    }
  };

  /**
   * The TimelineModel being viewed by the timeline
   * @type {TimelineModel}
   */
  cr.defineProperty(Timeline, 'model', cr.PropertyKind.JS);

  return {
    Timeline: Timeline,
    TimelineViewport: TimelineViewport
  };
});
