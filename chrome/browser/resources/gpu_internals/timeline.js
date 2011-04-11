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
cr.define('gpu', function() {

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
      this.viewport_.addEventListener('change', this.invalidate.bind(this));

      this.invalidatePending_ = false;

      this.tracks_ = this.ownerDocument.createElement('div');
      this.tracks_.invalidate = this.invalidate.bind(this);
      this.appendChild(this.tracks_);

      this.dragBox_ = this.ownerDocument.createElement('div');
      this.dragBox_.className = 'timeline-drag-box';
      this.appendChild(this.dragBox_);

      // The following code uses a setInterval to monitor the timeline control
      // for size changes. This is so that we can keep the canvas' bitmap size
      // correctly synchronized with its presentation size.
      // TODO(nduca): detect this in a more efficient way, e.g. iframe hack.
      this.lastSize_ = this.clientWidth + 'x' + this.clientHeight;
      this.ownerDocument.defaultView.setInterval(function() {
        var curSize = this.clientWidth + 'x' + this.clientHeight;
        if (this.clientWidth && curSize != this.lastSize_) {
          this.lastSize_ = curSize;
          this.onResize();
        }
      }.bind(this), 250);

      document.addEventListener('keypress', this.onKeypress_.bind(this));
      this.addEventListener('mousedown', this.onMouseDown_.bind(this));
      this.addEventListener('mousemove', this.onMouseMove_.bind(this));
      this.addEventListener('mouseup', this.onMouseUp_.bind(this));
      this.lastMouseViewPos_ = {x: 0, y: 0};

      this.selection_ = [];
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

      // Create tracks.
      this.tracks_.textContent = '';
      var threads = model.getAllThreads();
      for (var tI = 0; tI < threads.length; tI++) {
        var thread = threads[tI];
        var track = new TimelineThreadTrack();
        track.thread = thread;
        track.viewport = this.viewport_;
        this.tracks_.appendChild(track);

      }

      this.needsViewportReset_ = true;
    },

    invalidate: function() {
      if (this.invalidatePending_)
        return;
      this.invalidatePending_ = true;
      window.setTimeout(function() {
        this.invalidatePending_ = false;
        this.redrawAllTracks_();
      }.bind(this), 0);
    },

    onResize: function() {
      for (var i = 0; i < this.tracks_.children.length; ++i) {
        var track = this.tracks_.children[i];
        track.onResize();
      }
    },

    redrawAllTracks_: function() {
      if (this.needsViewportReset_ && this.clientWidth != 0) {
        this.needsViewportReset_ = false;
        /* update viewport */
        var rangeTimestamp = this.model_.maxTimestamp -
            this.model_.minTimestamp;
        var w = this.firstCanvas.width;
        console.log('viewport was reset with w=', w);
        var scaleX = w / rangeTimestamp;
        var panX = -this.model_.minTimestamp;
        this.viewport_.setPanAndScale(panX, scaleX);
      }
      for (var i = 0; i < this.tracks_.children.length; ++i) {
        this.tracks_.children[i].redraw();
      }
    },

    updateChildViewports_: function() {
      for (var cI = 0; cI < this.tracks_.children.length; ++cI) {
        var child = this.tracks_.children[cI];
        child.setViewport(this.panX, this.scaleX);
      }
    },

    onKeypress_: function(e) {
      var vp = this.viewport_;
      if (this.firstCanvas) {
        var viewWidth = this.firstCanvas.clientWidth;
        var curMouseV, curCenterW;
        switch (event.keyCode) {
          case 101: // e
            var vX = this.lastMouseViewPos_.x;
            var wX = vp.xViewToWorld(this.lastMouseViewPos_.x);
            var distFromCenter = vX - (viewWidth / 2);
            var percFromCenter = distFromCenter / viewWidth;
            var percFromCenterSq = percFromCenter * percFromCenter;
            vp.xPanWorldPosToViewPos(wX, 'center', viewWidth);
            break;
          case 119:  // w
            curMouseV = this.lastMouseViewPos_.x;
            curCenterW = vp.xViewToWorld(curMouseV);
            vp.scaleX = vp.scaleX * 1.5;
            vp.xPanWorldPosToViewPos(curCenterW, curMouseV, viewWidth);
            break;
          case 115:  // s
            curMouseV = this.lastMouseViewPos_.x;
            curCenterW = vp.xViewToWorld(curMouseV);
            vp.scaleX = vp.scaleX / 1.5;
            vp.xPanWorldPosToViewPos(curCenterW, curMouseV, viewWidth);
            break;
          case 87:  // W
            curMouseV = this.lastMouseViewPos_.x;
            curCenterW = vp.xViewToWorld(curMouseV);
            vp.scaleX = vp.scaleX * 10;
            vp.xPanWorldPosToViewPos(curCenterW, curMouseV, viewWidth);
            break;
          case 83:  // S
            curMouseV = this.lastMouseViewPos_.x;
            curCenterW = vp.xViewToWorld(curMouseV);
            vp.scaleX = vp.scaleX / 10;
            vp.xPanWorldPosToViewPos(curCenterW, curMouseV, viewWidth);
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
      }
    },

    get keyHelp() {
      return 'Keyboard shortcuts:\n' +
          ' w/s   : Zoom in/out\n' +
          ' a/d   : Pan left/right\n' +
          ' e     : Center on mouse';
    },

    get selection() {
      return this.selection_;
    },

    get firstCanvas() {
      return this.tracks_.firstChild ?
          this.tracks_.firstChild.firstCanvas : undefined;
    },

    showDragBox_: function() {
      this.dragBox_.hidden = false;
    },

    hideDragBox_: function() {
      this.dragBox_.hidden = true;
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

    onMouseDown_: function(e) {
      var canv = this.firstCanvas;
      var pos = {
        x: e.clientX - canv.offsetLeft,
        y: e.clientY - canv.offsetTop
      };
      var wX = this.viewport_.xViewToWorld(pos.x);

      // Update the drag box position
      this.showDragBox_();
      this.setDragBoxPosition_(e, e);
      this.dragBeginEvent_ = e;
      e.preventDefault();
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

        // Clear old selection.
        for (i = 0; i < this.selection_.length; ++i) {
          this.selection_[i].slice.selected = false;
        }
        // Figure out what has been hit.
        var selection = [];
        function addHit(type, track, slice) {
          selection.push({track: track, slice: slice});
        }
        for (i = 0; i < this.tracks_.children.length; ++i) {
          var track = this.tracks_.children[i];

          // Only check tracks that insersect the rect.
          var a = Math.max(loY, track.offsetTop);
          var b = Math.min(hiY, track.offsetTop + track.offsetHeight);
          if (a <= b) {
            track.pickRange(loWX, hiWX, loY, hiY, addHit);
          }
        }
        // Activate the new selection.
        this.selection_ = selection;
        cr.dispatchSimpleEvent(this, 'selectionChange');
        for (i = 0; i < this.selection_.length; ++i) {
          this.selection_[i].slice.selected = true;
        }
        this.invalidate();  // Cause tracks to redraw.
      }
    }
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
