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
    {r: 0x45, g: 0x85, b: 0xaa},
    {r: 0xdc, g: 0x73, b: 0xa8},
    {r: 0x77, g: 0xb6, b: 0x94},
    {r: 0x23, g: 0xae, b: 0x6e},
    {r: 0x76, g: 0x5d, b: 0x9e},
    {r: 0x48, g: 0xd8, b: 0xfb},
    {r: 0xa9, g: 0xd7, b: 0x93},
    {r: 0x7c, g: 0x2d, b: 0x52},
    {r: 0x69, g: 0xc2, b: 0x75},
    {r: 0x76, g: 0xcf, b: 0xee},
    {r: 0x3d, g: 0x85, b: 0xd1},
    {r: 0x71, g: 0x0b, b: 0x54}];

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

  /**
   * Creates a new timeline track div element
   * @constructor
   * @extends {HTMLDivElement}
   */
  TimelineTrack = cr.ui.define('div');

  TimelineTrack.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      this.classList.add('timeline-track');
      this.slices_ = null;

      this.titleDiv_ = document.createElement('div');
      this.titleDiv_.className = 'timeline-track-title';
      this.appendChild(this.titleDiv_);

      this.canvasContainer_ = document.createElement('div');
      this.canvasContainer_.className = 'timeline-track-canvas-container';
      this.appendChild(this.canvasContainer_);
      this.canvas_ = document.createElement('canvas');
      this.canvas_.className = 'timeline-track-canvas';
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
    get canvas() {
      return this.canvas_;
    },

    onResize: function() {
      this.canvas_.width = this.canvasContainer_.clientWidth;
      this.canvas_.height = this.canvasContainer_.clientHeight;
      this.invalidate();
    },

    invalidate: function() {
      if (this.parentNode)
        this.parentNode.invalidate();
    },

    set viewport(v) {
      this.viewport_ = v;
      this.invalidate();
    },

    getCanvasWidth: function() {
      return this.canvas_.width;
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
      var viewRWorld = vp.xViewToWorld(this.width);

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
    },

    /**
     * Picks a slice, if any, at a given location.
     * @param {number} wX Location to search at, in worldspace.
     * @param {function():*} onHitCallback Callback to call with the slice,
     *     if one is found.
     * @return {boolean} true if a slice was found, otherwise false.
     */
    pick: function(wX, onHitCallback) {
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
     * @param {number} loWX Lower bound of the interval to search, in
     *     worldspace.
     * @param {number} hiWX Upper bound of the interval to search, in
     *     worldspace.
     * @param {function():*} onHitCallback Function to call for each slice
     *     intersecting the interval.
     */
    pickRange: function(loWX, hiWX, onHitCallback) {
      function onPickHit(slice) {
        onHitCallback('slice', this, slice);
      }
      gpu.iterateOverIntersectingIntervals(this.slices_,
          function(x) { return x.start; },
          function(x) { return x.duration; },
          loWX, hiWX,
          onPickHit);
    }

  };

  return {
    TimelineTrack: TimelineTrack
  };
});
