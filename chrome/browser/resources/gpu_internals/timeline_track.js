// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Renders an array of slices into the provided div,
 * using a child canvas element. Uses a FastRectRenderer to draw only
 * the visible slices.
 */
cr.define('gpu', function() {

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

  function addTrack(thisTrack, slices) {
    var track = new TimelineSliceTrack();

    track.heading = '';
    track.slices = slices;
    track.viewport = thisTrack.viewport_;

    thisTrack.tracks_.push(track);
    thisTrack.appendChild(track);
  }

  /**
   * Generic base class for timeline tracks
   */
  TimelineThreadTrack = cr.ui.define('div');
  TimelineThreadTrack.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      this.className = 'timeline-thread-track';
    },

    set thread(thread) {
      this.thread_ = thread;
      this.updateChildTracks_();
    },

    set viewport(v) {
      this.viewport_ = v;
      for (var i = 0; i < this.tracks_.length; i++)
        this.tracks_[i].viewport = v;
      this.invalidate();
    },

    invalidate: function() {
      if (this.parentNode)
        this.parentNode.invalidate();
    },

    onResize: function() {
      for (var i = 0; i < this.tracks_.length; i++)
        this.tracks_[i].onResize();
    },

    get firstCanvas() {
      if (this.tracks_.length)
        return this.tracks_[0].firstCanvas;
      return undefined;
    },

    redraw: function() {
      for (var i = 0; i < this.tracks_.length; i++)
        this.tracks_[i].redraw();
    },

    updateChildTracks_: function() {
      this.textContent = '';
      this.tracks_ = [];
      if (this.thread_) {
        for (var srI = 0; srI < this.thread_.nonNestedSubRows.length; ++srI) {
          addTrack(this, this.thread_.nonNestedSubRows[srI]);
        }
        for (var srI = 0; srI < this.thread_.subRows.length; ++srI) {
          addTrack(this, this.thread_.subRows[srI]);
        }
        if (this.tracks_.length > 0) {
          this.tracks_[0].heading = this.thread_.parent.pid + ': ' +
              this.thread_.tid + ': ';
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
   * Creates a new timeline track div element
   * @constructor
   * @extends {HTMLDivElement}
   */
  TimelineSliceTrack = cr.ui.define('div');

  TimelineSliceTrack.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      this.className = 'timeline-slice-track';
      this.slices_ = null;

      this.titleDiv_ = document.createElement('div');
      this.titleDiv_.className = 'timeline-slice-track-title';
      this.appendChild(this.titleDiv_);

      this.canvasContainer_ = document.createElement('div');
      this.canvasContainer_.className = 'timeline-slice-track-canvas-container';
      this.appendChild(this.canvasContainer_);
      this.canvas_ = document.createElement('canvas');
      this.canvas_.className = 'timeline-slice-track-canvas';
      this.canvasContainer_.appendChild(this.canvas_);

      this.ctx_ = this.canvas_.getContext('2d');
    },

    set heading(text) {
      this.titleDiv_.textContent = text;
    },

    set slices(slices) {
      this.slices_ = slices;
      this.invalidate();
    },

    set viewport(v) {
      this.viewport_ = v;
      this.invalidate();
    },

    invalidate: function() {
      if (this.parentNode)
        this.parentNode.invalidate();
    },

    get firstCanvas() {
      return this.canvas_;
    },

    onResize: function() {
      this.canvas_.width = this.canvasContainer_.clientWidth;
      this.canvas_.height = this.canvasContainer_.clientHeight;
      this.invalidate();
    },

    redraw: function() {
      if (!this.viewport_)
        return;
      var ctx = this.ctx_;
      var canvasW = this.canvas_.width;
      var canvasH = this.canvas_.height;

      ctx.clearRect(0, 0, canvasW, canvasH);

      // culling...
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

      // begin rendering in world space
      ctx.save();
      vp.applyTransformToCanavs(ctx);

      // tracks
      var tr = new gpu.FastRectRenderer(ctx, viewLWorld, 2 * pixWidth,
                                        2 * pixWidth, viewRWorld, pallette);
      tr.setYandH(0, canvasH);
      var slices = this.slices_;
      for (var i = 0; i < slices.length; ++i) {
        var slice = slices[i];
        var x = slice.start;
        var w = slice.duration;
        var colorId;
        colorId = slice.selected ?
            slice.colorId + selectedIdBoost :
            slice.colorId;

        if (w < pixWidth)
          w = pixWidth;
        tr.fillRect(x, w, colorId);
      }
      tr.flush();
      ctx.restore();

      // labels
      ctx.textAlign = 'center';
      ctx.textBaseline = 'top';
      ctx.font = '10px sans-serif';
      ctx.strokeStyle = 'rgb(0,0,0)';
      ctx.fillStyle = 'rgb(0,0,0)';
      var quickDiscardThresshold = pixWidth * 20; // dont render until 20px wide
      for (var i = 0; i < slices.length; ++i) {
        var slice = slices[i];
        if (slice.duration > quickDiscardThresshold) {
          var labelWidth = quickMeasureText(ctx, slice.title) + 2;
          var labelWidthWorld = pixWidth * labelWidth;
          if (labelWidthWorld < slice.duration) {
            var cX = vp.xWorldToView(slice.start + 0.5 * slice.duration);
            ctx.fillText(slice.title, cX, 2.5);
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
      var x = gpu.findLowIndexInSortedIntervals(this.slices_,
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
      gpu.iterateOverIntersectingIntervals(this.slices_,
          function(x) { return x.start; },
          function(x) { return x.duration; },
          loWX, hiWX,
          onPickHit);
    }

  };

  return {
    TimelineSliceTrack: TimelineSliceTrack,
    TimelineThreadTrack: TimelineThreadTrack
  };
});
