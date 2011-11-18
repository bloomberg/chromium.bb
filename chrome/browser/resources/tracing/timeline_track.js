// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Renders an array of slices into the provided div,
 * using a child canvas element. Uses a FastRectRenderer to draw only
 * the visible slices.
 */
cr.define('tracing', function() {

  const palletteBase = [
    {r: 138, g: 113, b: 152},
    {r: 175, g: 112, b: 133},
    {r: 127, g: 135, b: 225},
    {r: 93, g: 81, b: 137},
    {r: 116, g: 143, b: 119},
    {r: 178, g: 214, b: 122},
    {r: 87, g: 109, b: 147},
    {r: 119, g: 155, b: 95},
    {r: 114, g: 180, b: 160},
    {r: 132, g: 85, b: 103},
    {r: 157, g: 210, b: 150},
    {r: 148, g: 94, b: 86},
    {r: 164, g: 108, b: 138},
    {r: 139, g: 191, b: 150},
    {r: 110, g: 99, b: 145},
    {r: 80, g: 129, b: 109},
    {r: 125, g: 140, b: 149},
    {r: 93, g: 124, b: 132},
    {r: 140, g: 85, b: 140},
    {r: 104, g: 163, b: 162},
    {r: 132, g: 141, b: 178},
    {r: 131, g: 105, b: 147},
    {r: 135, g: 183, b: 98},
    {r: 152, g: 134, b: 177},
    {r: 141, g: 188, b: 141},
    {r: 133, g: 160, b: 210},
    {r: 126, g: 186, b: 148},
    {r: 112, g: 198, b: 205},
    {r: 180, g: 122, b: 195},
    {r: 203, g: 144, b: 152}];

  function brighten(c) {
    return {r: Math.min(255, c.r + Math.floor(c.r * 0.45)),
      g: Math.min(255, c.g + Math.floor(c.g * 0.45)),
      b: Math.min(255, c.b + Math.floor(c.b * 0.45))};
  }
  function colorToString(c) {
    return 'rgb(' + c.r + ',' + c.g + ',' + c.b + ')';
  }

  const selectedIdBoost = palletteBase.length;

  const pallette = palletteBase.concat(palletteBase.map(brighten)).
      map(colorToString);

  var textWidthMap = { };
  function quickMeasureText(ctx, text) {
    var w = textWidthMap[text];
    if (!w) {
      w = ctx.measureText(text).width;
      textWidthMap[text] = w;
    }
    return w;
  }

  /**
   * A generic track that contains other tracks as its children.
   * @constructor
   */
  var TimelineContainerTrack = cr.ui.define('div');
  TimelineContainerTrack.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      this.tracks_ = [];
    },

    detach: function() {
      for (var i = 0; i < this.tracks_.length; i++)
        this.tracks_[i].detach();
    },

    get viewport() {
      return this.viewport_;
    },

    set viewport(v) {
      this.viewport_ = v;
      for (var i = 0; i < this.tracks_.length; i++)
        this.tracks_[i].viewport = v;
      this.updateChildTracks_();
    },

    get firstCanvas() {
      if (this.tracks_.length)
        return this.tracks_[0].firstCanvas;
      return undefined;
    },

    /**
     * Picks a slice, if any, at a given location.
     * @param {number} wX X location to search at, in worldspace.
     * @param {number} wY Y location to search at, in offset space.
     *     offset space.
     * @param {function():*} onHitCallback Callback to call with the slice,
     *     if one is found.
     * @return {boolean} true if a slice was found, otherwise false.
     */
    pick: function(wX, wY, onHitCallback) {
      for (var i = 0; i < this.tracks_.length; i++) {
        var trackClientRect = this.tracks_[i].getBoundingClientRect();
        if (wY >= trackClientRect.top && wY < trackClientRect.bottom)
          return this.tracks_[i].pick(wX, onHitCallback);
      }
      return false;
    },

    /**
     * Finds slices intersecting the given interval.
     * @param {number} loWX Lower X bound of the interval to search, in
     *     worldspace.
     * @param {number} hiWX Upper X bound of the interval to search, in
     *     worldspace.
     * @param {number} loY Lower Y bound of the interval to search, in
     *     offset space.
     * @param {number} hiY Upper Y bound of the interval to search, in
     *     offset space.
     * @param {function():*} onHitCallback Function to call for each slice
     *     intersecting the interval.
     */
    pickRange: function(loWX, hiWX, loY, hiY, onHitCallback) {
      for (var i = 0; i < this.tracks_.length; i++) {
        var trackClientRect = this.tracks_[i].getBoundingClientRect();
        var a = Math.max(loY, trackClientRect.top);
        var b = Math.min(hiY, trackClientRect.bottom);
        if (a <= b)
          this.tracks_[i].pickRange(loWX, hiWX, loY, hiY, onHitCallback);
      }
    }
  };

  /**
   * Visualizes a TimelineThread using a series of of TimelineSliceTracks.
   * @constructor
   */
  var TimelineThreadTrack = cr.ui.define(TimelineContainerTrack);
  TimelineThreadTrack.prototype = {
    __proto__: TimelineContainerTrack.prototype,

    decorate: function() {
      this.classList.add('timeline-thread-track');
    },

    get thread(thread) {
      return this.thread_;
    },

    set thread(thread) {
      this.thread_ = thread;
      this.updateChildTracks_();
    },

    get tooltip() {
      return this.tooltip_;
    },

    set tooltip(value) {
      this.tooltip_ = value;
      this.updateChildTracks_();
    },

    get heading() {
      return this.heading_;
    },

    set heading(h) {
      this.heading_ = h;
      this.updateChildTracks_();
    },

    get headingWidth() {
      return this.headingWidth_;
    },

    set headingWidth(width) {
      this.headingWidth_ = width;
      this.updateChildTracks_();
    },

    addTrack_: function(slices) {
      var track = new TimelineSliceTrack();
      track.heading = '';
      track.slices = slices;
      track.headingWidth = this.headingWidth_;
      track.viewport = this.viewport_;

      this.tracks_.push(track);
      this.appendChild(track);
    },

    updateChildTracks_: function() {
      this.detach();
      this.textContent = '';
      this.tracks_ = [];
      if (this.thread_) {
        for (var srI = 0; srI < this.thread_.nonNestedSubRows.length; ++srI) {
          this.addTrack_(this.thread_.nonNestedSubRows[srI]);
        }
        for (var srI = 0; srI < this.thread_.subRows.length; ++srI) {
          this.addTrack_(this.thread_.subRows[srI]);
        }
        if (this.tracks_.length > 0) {
          this.tracks_[0].heading = this.heading_;
          this.tracks_[0].tooltip = this.tooltip_;
        }
      }
    }
  };

  /**
   * A canvas-based track constructed. Provides the basic heading and
   * invalidation-managment infrastructure. Subclasses must implement drawing
   * and picking code.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var CanvasBasedTrack = cr.ui.define('div');

  CanvasBasedTrack.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      this.className = 'timeline-canvas-based-track';
      this.slices_ = null;

      this.headingDiv_ = document.createElement('div');
      this.headingDiv_.className = 'timeline-canvas-based-track-title';
      this.appendChild(this.headingDiv_);

      this.canvasContainer_ = document.createElement('div');
      this.canvasContainer_.className =
          'timeline-canvas-based-track-canvas-container';
      this.appendChild(this.canvasContainer_);
      this.canvas_ = document.createElement('canvas');
      this.canvas_.className = 'timeline-canvas-based-track-canvas';
      this.canvasContainer_.appendChild(this.canvas_);

      this.ctx_ = this.canvas_.getContext('2d');
    },

    detach: function() {
      if (this.viewport_)
        this.viewport_.removeEventListener('change',
                                           this.viewportChangeBoundToThis_);
    },

    set headingWidth(width) {
      this.headingDiv_.style.width = width;
    },

    get heading() {
      return this.headingDiv_.textContent;
    },

    set heading(text) {
      this.headingDiv_.textContent = text;
    },

    set tooltip(text) {
      this.headingDiv_.title = text;
    },

    get viewport() {
      return this.viewport_;
    },

    set viewport(v) {
      this.viewport_ = v;
      if (this.viewport_)
        this.viewport_.removeEventListener('change',
                                           this.viewportChangeBoundToThis_);
      this.viewport_ = v;
      if (this.viewport_) {
        this.viewportChangeBoundToThis_ = this.viewportChange_.bind(this);
        this.viewport_.addEventListener('change',
                                        this.viewportChangeBoundToThis_);
      }
      this.invalidate();
    },

    viewportChange_: function() {
      this.invalidate();
    },

    invalidate: function() {
      if (this.rafPending_)
        return;
      webkitRequestAnimationFrame(function() {
        this.rafPending_ = false;
        if (!this.viewport_)
          return;

        if (this.canvas_.width != this.canvasContainer_.clientWidth)
          this.canvas_.width = this.canvasContainer_.clientWidth;
        if (this.canvas_.height != this.canvasContainer_.clientHeight)
          this.canvas_.height = this.canvasContainer_.clientHeight;

        this.redraw();
      }.bind(this), this);
      this.rafPending_ = true;
    },

    get firstCanvas() {
      return this.canvas_;
    }

  };

  /**
   * A track that displays an array of TimelineSlice objects.
   * @constructor
   * @extends {CanvasBasedTrack}
   */

  var TimelineSliceTrack = cr.ui.define(CanvasBasedTrack);

  TimelineSliceTrack.prototype = {

    __proto__: CanvasBasedTrack.prototype,

    decorate: function() {
      this.classList.add('timeline-slice-track');
    },

    get slices() {
      return this.slices_;
    },

    set slices(slices) {
      this.slices_ = slices;
      this.invalidate();
    },

    redraw: function() {
      var ctx = this.ctx_;
      var canvasW = this.canvas_.width;
      var canvasH = this.canvas_.height;

      ctx.clearRect(0, 0, canvasW, canvasH);

      // Culling parameters.
      var vp = this.viewport_;
      var pixWidth = vp.xViewVectorToWorld(1);
      var viewLWorld = vp.xViewToWorld(0);
      var viewRWorld = vp.xViewToWorld(canvasW);

      // Draw grid without a transform because the scale
      // affects line width.
      if (vp.gridEnabled) {
        var x = vp.gridTimebase;
        ctx.beginPath();
        while (x < viewRWorld) {
          if (x >= viewLWorld) {
            // Do conversion to viewspace here rather than on
            // x to avoid precision issues.
            var vx = vp.xWorldToView(x);
            ctx.moveTo(vx, 0);
            ctx.lineTo(vx, canvasH);
          }
          x += vp.gridStep;
        }
        ctx.strokeStyle = 'rgba(255,0,0,0.25)';
        ctx.stroke();
      }

      // Begin rendering in world space.
      ctx.save();
      vp.applyTransformToCanavs(ctx);

      // Slices.
      var tr = new tracing.FastRectRenderer(ctx, viewLWorld, 2 * pixWidth,
                                            2 * pixWidth, viewRWorld, pallette);
      tr.setYandH(0, canvasH);
      var slices = this.slices_;
      for (var i = 0; i < slices.length; ++i) {
        var slice = slices[i];
        var x = slice.start;
        // Less than 0.001 causes short events to disappear when zoomed in.
        var w = Math.max(slice.duration, 0.001);
        var colorId;
        colorId = slice.selected ?
            slice.colorId + selectedIdBoost :
            slice.colorId;

        if (w < pixWidth)
          w = pixWidth;
        if (slice.duration > 0) {
          tr.fillRect(x, w, colorId);
        } else {
          // Instant: draw a triangle.  If zoomed too far, collapse
          // into the FastRectRenderer.
          if (pixWidth > 0.001) {
            tr.fillRect(x, pixWidth, colorId);
          } else {
            ctx.fillStyle = pallette[colorId];
            ctx.beginPath();
            ctx.moveTo(x - (4 * pixWidth), canvasH);
            ctx.lineTo(x, 0);
            ctx.lineTo(x + (4 * pixWidth), canvasH);
            ctx.closePath();
            ctx.fill();
          }
        }
      }
      tr.flush();
      ctx.restore();

      // Labels.
      ctx.textAlign = 'center';
      ctx.textBaseline = 'top';
      ctx.font = '10px sans-serif';
      ctx.strokeStyle = 'rgb(0,0,0)';
      ctx.fillStyle = 'rgb(0,0,0)';
      var quickDiscardThresshold = pixWidth * 20; // dont render until 20px wide
      for (var i = 0; i < slices.length; ++i) {
        var slice = slices[i];
        if (slice.duration > quickDiscardThresshold) {
          var title = slice.title;
          if (slice.didNotFinish) {
            title += ' (Did Not Finish)';
          }
          var labelWidth = quickMeasureText(ctx, title) + 2;
          var labelWidthWorld = pixWidth * labelWidth;

          if (labelWidthWorld < slice.duration) {
            var cX = vp.xWorldToView(slice.start + 0.5 * slice.duration);
            ctx.fillText(title, cX, 2.5, labelWidth);
          }
        }
      }
    },

    /**
     * Picks a slice, if any, at a given location.
     * @param {number} wX X location to search at, in worldspace.
     * @param {number} wY Y location to search at, in offset space.
     *     offset space.
     * @param {function():*} onHitCallback Callback to call with the slice,
     *     if one is found.
     * @return {boolean} true if a slice was found, otherwise false.
     */
    pick: function(wX, wY, onHitCallback) {
      var clientRect = this.getBoundingClientRect();
      if (wY < clientRect.top || wY >= clientRect.bottom)
        return false;
      var x = tracing.findLowIndexInSortedIntervals(this.slices_,
          function(x) { return x.start; },
          function(x) { return x.duration; },
          wX);
      if (x >= 0 && x < this.slices_.length) {
        onHitCallback('slice', this, this.slices_[x]);
        return true;
      }
      return false;
    },

    /**
     * Finds slices intersecting the given interval.
     * @param {number} loWX Lower X bound of the interval to search, in
     *     worldspace.
     * @param {number} hiWX Upper X bound of the interval to search, in
     *     worldspace.
     * @param {number} loY Lower Y bound of the interval to search, in
     *     offset space.
     * @param {number} hiY Upper Y bound of the interval to search, in
     *     offset space.
     * @param {function():*} onHitCallback Function to call for each slice
     *     intersecting the interval.
     */
    pickRange: function(loWX, hiWX, loY, hiY, onHitCallback) {
      var clientRect = this.getBoundingClientRect();
      var a = Math.max(loY, clientRect.top);
      var b = Math.min(hiY, clientRect.bottom);
      if (a > b)
        return;

      var that = this;
      function onPickHit(slice) {
        onHitCallback('slice', that, slice);
      }
      tracing.iterateOverIntersectingIntervals(this.slices_,
          function(x) { return x.start; },
          function(x) { return x.duration; },
          loWX, hiWX,
          onPickHit);
    },

    /**
     * Find the index for the given slice.
     * @return {index} Index of the given slice, or undefined.
     * @private
     */
    indexOfSlice_: function(slice) {
      var index = tracing.findLowIndexInSortedArray(this.slices_,
          function(x) { return x.start; },
          slice.start);
      while (index < this.slices_.length &&
          slice.start == this.slices_[index].start &&
          slice.colorId != this.slices_[index].colorId) {
        index++;
      }
      return index < this.slices_.length ? index : undefined;
    },

    /**
     * Return the next slice, if any, after the given slice.
     * @param {slice} The previous slice.
     * @return {slice} The next slice, or undefined.
     * @private
     */
    pickNext: function(slice) {
      var index = this.indexOfSlice_(slice);
      if (index != undefined) {
        if (index < this.slices_.length - 1)
          index++;
        else
          index = undefined;
      }
      return index != undefined ? this.slices_[index] : undefined;
    },

    /**
     * Return the previous slice, if any, before the given slice.
     * @param {slice} A slice.
     * @return {slice} The previous slice, or undefined.
     */
    pickPrevious: function(slice) {
      var index = this.indexOfSlice_(slice);
      if (index == 0)
        return undefined;
      else if ((index != undefined) && (index > 0))
        index--;
      return index != undefined ? this.slices_[index] : undefined;
    }

  };

  /**
   * A track that displays a TimelineCounter object.
   * @constructor
   * @extends {CanvasBasedTrack}
   */

  var TimelineCounterTrack = cr.ui.define(CanvasBasedTrack);

  TimelineCounterTrack.prototype = {

    __proto__: CanvasBasedTrack.prototype,

    decorate: function() {
      this.classList.add('timeline-counter-track');
    },

    get counter() {
      return this.counter_;
    },

    set counter(counter) {
      this.counter_ = counter;
      this.invalidate();
    },

    redraw: function() {
      var ctr = this.counter_;
      var ctx = this.ctx_;
      var canvasW = this.canvas_.width;
      var canvasH = this.canvas_.height;

      ctx.clearRect(0, 0, canvasW, canvasH);

      // Culling parametrs.
      var vp = this.viewport_;
      var pixWidth = vp.xViewVectorToWorld(1);
      var viewLWorld = vp.xViewToWorld(0);
      var viewRWorld = vp.xViewToWorld(canvasW);

      // Drop sampels that are less than skipDistancePix apart.
      var skipDistancePix = 16;
      var skipDistanceWorld = vp.xViewVectorToWorld(skipDistancePix);

      // Begin rendering in world space.
      ctx.save();
      vp.applyTransformToCanavs(ctx);

      // Figure out where drawing should begin.
      var numSeries = ctr.numSeries;
      var numSamples = ctr.numSamples;
      var startIndex = tracing.findLowIndexInSortedArray(ctr.timestamps,
                                                         function() {
                                                         },
                                                         viewLWorld);

      // Draw indices one by one until we fall off the viewRWorld.
      var yScale = canvasH / ctr.maxTotal;
      for (var seriesIndex = ctr.numSeries - 1;
           seriesIndex >= 0; seriesIndex--) {
        var colorId = ctr.seriesColors[seriesIndex];
        ctx.fillStyle = pallette[colorId];
        ctx.beginPath();

        // Set iLast and xLast such that the first sample we draw is the
        // startIndex sample.
        var iLast = startIndex - 1;
        var xLast = iLast >= 0 ? ctr.timestamps[iLast] - skipDistanceWorld : -1;
        var yLastView = canvasH;

        // Iterate over samples from iLast onward until we either fall off the
        // viewRWorld or we run out of samples. To avoid drawing too much, after
        // drawing a sample at xLast, skip subsequent samples that are less than
        // skipDistanceWorld from xLast.
        var hasMoved = false;
        while (true) {
          var i = iLast + 1;
          if (i >= numSamples) {
            ctx.lineTo(xLast, yLastView);
            ctx.lineTo(xLast + 8 * pixWidth, yLastView);
            ctx.lineTo(xLast + 8 * pixWidth, canvasH);
            break;
          }

          var x = ctr.timestamps[i];

          var y = ctr.totals[i * numSeries + seriesIndex];
          var yView = canvasH - (yScale * y);

          if (x > viewRWorld) {
            ctx.lineTo(x, yLastView);
            ctx.lineTo(x, canvasH);
            break;
          }

          if (x - xLast < skipDistanceWorld) {
            iLast = i;
            continue;
          }

          if (!hasMoved) {
            ctx.moveTo(viewLWorld, canvasH);
            hasMoved = true;
          }
          ctx.lineTo(x, yLastView);
          ctx.lineTo(x, yView);
          iLast = i;
          xLast = x;
          yLastView = yView;
        }
        ctx.closePath();
        ctx.fill();
      }
      ctx.restore();
    },

    /**
     * Picks a slice, if any, at a given location.
     * @param {number} wX X location to search at, in worldspace.
     * @param {number} wY Y location to search at, in offset space.
     *     offset space.
     * @param {function():*} onHitCallback Callback to call with the slice,
     *     if one is found.
     * @return {boolean} true if a slice was found, otherwise false.
     */
    pick: function(wX, wY, onHitCallback) {
    },

    /**
     * Finds slices intersecting the given interval.
     * @param {number} loWX Lower X bound of the interval to search, in
     *     worldspace.
     * @param {number} hiWX Upper X bound of the interval to search, in
     *     worldspace.
     * @param {number} loY Lower Y bound of the interval to search, in
     *     offset space.
     * @param {number} hiY Upper Y bound of the interval to search, in
     *     offset space.
     * @param {function():*} onHitCallback Function to call for each slice
     *     intersecting the interval.
     */
    pickRange: function(loWX, hiWX, loY, hiY, onHitCallback) {
    }

  };

  return {
    TimelineCounterTrack: TimelineCounterTrack,
    TimelineSliceTrack: TimelineSliceTrack,
    TimelineThreadTrack: TimelineThreadTrack
  };
});
