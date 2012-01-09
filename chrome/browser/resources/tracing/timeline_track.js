// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Renders an array of slices into the provided div,
 * using a child canvas element. Uses a FastRectRenderer to draw only
 * the visible slices.
 */
cr.define('tracing', function() {

  var pallette = tracing.getPallette();
  var highlightIdBoost = tracing.getPalletteHighlightIdBoost();

  // TODO(jrg): possibly obsoleted with the elided string cache.
  // Consider removing.
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
   * Cache for elided strings.
   * Moved from the ElidedTitleCache protoype to a "global" for speed
   * (variable reference is 100x faster).
   *   key: String we wish to elide.
   *   value: Another dict whose key is width
   *     and value is an ElidedStringWidthPair.
   */
  var elidedTitleCacheDict = {};

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
      return track;
    },

    updateChildTracks_: function() {
      this.detach();
      this.textContent = '';
      this.tracks_ = [];
      if (this.thread_) {
        if (this.thread_.cpuSlices) {
          var track = this.addTrack_(this.thread_.cpuSlices);
          track.height = '4px';
        }

        for (var srI = 0; srI < this.thread_.nonNestedSubRows.length; ++srI) {
          this.addTrack_(this.thread_.nonNestedSubRows[srI]);
        }
        for (var srI = 0; srI < this.thread_.subRows.length; ++srI) {
          this.addTrack_(this.thread_.subRows[srI]);
        }
        if (this.tracks_.length > 0) {
          if (this.thread_.cpuSlices) {
            this.tracks_[1].heading = this.heading_;
            this.tracks_[1].tooltip = this.tooltip_;
          } else {
            this.tracks_[0].heading = this.heading_;
            this.tracks_[0].tooltip = this.tooltip_;
          }
        }
      }
    }
  };

  /**
   * Visualizes a TimelineCpu using a series of of TimelineSliceTracks.
   * @constructor
   */
  var TimelineCpuTrack = cr.ui.define(TimelineContainerTrack);
  TimelineCpuTrack.prototype = {
    __proto__: TimelineContainerTrack.prototype,

    decorate: function() {
      this.classList.add('timeline-thread-track');
    },

    get cpu(cpu) {
      return this.cpu_;
    },

    set cpu(cpu) {
      this.cpu_ = cpu;
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

    updateChildTracks_: function() {
      this.detach();
      this.textContent = '';
      this.tracks_ = [];
      if (this.cpu_) {
        var track = new TimelineSliceTrack();
        track.slices = this.cpu_.slices;
        track.headingWidth = this.headingWidth_;
        track.viewport = this.viewport_;

        this.tracks_.push(track);
        this.appendChild(track);

        this.tracks_[0].heading = this.heading_;
        this.tracks_[0].tooltip = this.tooltip_;
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
     * A pair representing an elided string and world-coordinate width
     * to draw it.
     * @constructor
     */
   function ElidedStringWidthPair(string, width) {
     this.string = string;
     this.width = width;
   }

    /**
    * A cache for elided strings.
    * @constructor
    */
   function ElidedTitleCache() {
   }

   ElidedTitleCache.prototype = {
     /**
      * Return elided text.
      * @param {track} A timeline slice track or other object that defines
      *                functions labelWidth() and labelWidthWorld().
      * @param {pixWidth} Pixel width.
      * @param {title} Original title text.
      * @param {width} Drawn width in world coords.
      * @param {sliceDuration} Where the title must fit (in world coords).
      * @return {ElidedStringWidthPair} Elided string and width.
      */
     get: function(track, pixWidth, title, width, sliceDuration) {
       var elidedDict = elidedTitleCacheDict[title];
       if (!elidedDict) {
         elidedDict = {};
         elidedTitleCacheDict[title] = elidedDict;
       }
       var stringWidthPair = elidedDict[sliceDuration];
       if (stringWidthPair === undefined) {
          var newtitle = title;
          var elided = false;
          while (track.labelWidthWorld(newtitle, pixWidth) > sliceDuration) {
            newtitle = newtitle.substring(0, newtitle.length * 0.75);
            elided = true;
          }
          if (elided && newtitle.length > 3)
            newtitle = newtitle.substring(0, newtitle.length - 3) + '...';
         stringWidthPair = new ElidedStringWidthPair(
                             newtitle,
                             track.labelWidth(newtitle));
         elidedDict[sliceDuration] = stringWidthPair;
       }
       return stringWidthPair;
     },
   };

  /**
   * A track that displays an array of TimelineSlice objects.
   * @constructor
   * @extends {CanvasBasedTrack}
   */

  var TimelineSliceTrack = cr.ui.define(CanvasBasedTrack);

  TimelineSliceTrack.prototype = {

    __proto__: CanvasBasedTrack.prototype,

   /**
    * Should we elide text on trace labels?
    * Without eliding, text that is too wide isn't drawn at all.
    * Disable if you feel this causes a performance problem.
    * This is a default value that can be overridden in tracks for testing.
    * @const
    */
    SHOULD_ELIDE_TEXT: true,

    decorate: function() {
      this.classList.add('timeline-slice-track');
      this.elidedTitleCache = new ElidedTitleCache();
    },

    get slices() {
      return this.slices_;
    },

    set slices(slices) {
      this.slices_ = slices;
      this.invalidate();
    },

    set height(height) {
      this.style.height = height;
    },

    labelWidth: function(title) {
      return quickMeasureText(this.ctx_, title) + 2;
    },

    labelWidthWorld: function(title, pixWidth) {
      return this.labelWidth(title) * pixWidth;
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
        var colorId = slice.selected ?
            slice.colorId + highlightIdBoost :
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
      if (canvasH > 8) {
        ctx.textAlign = 'center';
        ctx.textBaseline = 'top';
        ctx.font = '10px sans-serif';
        ctx.strokeStyle = 'rgb(0,0,0)';
        ctx.fillStyle = 'rgb(0,0,0)';
        // Don't render text until until it is 20px wide
        var quickDiscardThresshold = pixWidth * 20;
        var shouldElide = this.SHOULD_ELIDE_TEXT;
        for (var i = 0; i < slices.length; ++i) {
          var slice = slices[i];
          if (slice.duration > quickDiscardThresshold) {
            var title = slice.title;
            if (slice.didNotFinish) {
              title += ' (Did Not Finish)';
            }
            var drawnTitle = title;
            var drawnWidth = this.labelWidth(drawnTitle);
            if (shouldElide &&
                this.labelWidthWorld(drawnTitle, pixWidth) > slice.duration) {
                var elidedValues = this.elidedTitleCache.get(
                    this, pixWidth,
                    drawnTitle, drawnWidth,
                    slice.duration);
                drawnTitle = elidedValues.string;
                drawnWidth = elidedValues.width;
            }
            if (drawnWidth * pixWidth < slice.duration) {
              var cX = vp.xWorldToView(slice.start + 0.5 * slice.duration);
              ctx.fillText(drawnTitle, cX, 2.5, drawnWidth);
            }
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
   * A track that displays the viewport size and scale.
   * @constructor
   * @extends {CanvasBasedTrack}
   */

  var TimelineViewportTrack = cr.ui.define(CanvasBasedTrack);

  const logOf10 = Math.log(10);
  function log10(x) {
    return Math.log(x) / logOf10;;
  }

  TimelineViewportTrack.prototype = {

    __proto__: CanvasBasedTrack.prototype,

    decorate: function() {
      this.classList.add('timeline-viewport-track');
      this.strings_ = {};
    },

    redraw: function() {
      var ctx = this.ctx_;
      var canvasW = this.canvas_.width;
      var canvasH = this.canvas_.height;

      ctx.clearRect(0, 0, canvasW, canvasH);

      // Culling parametrs.
      var vp = this.viewport_;
      var pixWidth = vp.xViewVectorToWorld(1);
      var viewLWorld = vp.xViewToWorld(0);
      var viewRWorld = vp.xViewToWorld(canvasW);

      var idealMajorMarkDistancePix = 150;
      var idealMajorMarkDistanceWorld =
          vp.xViewVectorToWorld(idealMajorMarkDistancePix);

      // The conservative guess is the nearest enclosing 0.1, 1, 10, 100, etc
      var conservativeGuess =
          Math.pow(10, Math.ceil(log10(idealMajorMarkDistanceWorld)));

      // Once we have a conservative guess, consider things that evenly add up
      // to the conservative guess, e.g. 0.5, 0.2, 0.1 Pick the one that still
      // exceeds the ideal mark distance.
      var divisors = [10, 5, 2, 1];
      for (var i = 0; i < divisors.length; ++i) {
        var tightenedGuess = conservativeGuess / divisors[i]
        if (vp.xWorldVectorToView(tightenedGuess) < idealMajorMarkDistancePix)
          continue;
        majorMarkDistanceWorld = conservativeGuess / divisors[i-1];
        break;
      }
      if (majorMarkDistanceWorld < 100) {
        unit = 'ms';
        unitDivisor = 1;
      } else {
        unit = 's';
        unitDivisor = 1000;
      }

      var numTicksPerMajor = 5;
      var minorMarkDistanceWorld = majorMarkDistanceWorld / numTicksPerMajor;
      var minorMarkDistancePx = vp.xWorldVectorToView(minorMarkDistanceWorld);

      var firstMajorMark =
        Math.floor(viewLWorld / majorMarkDistanceWorld) *
        majorMarkDistanceWorld;

      var minorTickH = Math.floor(canvasH * 0.25);

      ctx.fillStyle = 'rgb(0, 0, 0)';
      ctx.strokeStyle = 'rgb(0, 0, 0)';
      ctx.textAlign = 'left';
      ctx.textBaseline = 'top';
      ctx.font = '9px sans-serif';

      // Each iteration of this loop draws one major mark
      // and numTicksPerMajor minor ticks.
      //
      // Rendering can't be done in world space because canvas transforms
      // affect line width. So, do the conversions manually.
      for (var curX = firstMajorMark;
           curX < viewRWorld;
           curX += majorMarkDistanceWorld) {

        var curXView = Math.floor(vp.xWorldToView(curX));

        var unitValue = curX / unitDivisor;
        var roundedUnitValue = Math.floor(unitValue * 100000) / 100000;
        if (!this.strings_[roundedUnitValue])
          this.strings_[roundedUnitValue] = roundedUnitValue + ' ' + unit;
        ctx.fillText(this.strings_[roundedUnitValue], curXView + 2, 0);

        ctx.beginPath();

        // Major mark
        ctx.moveTo(curXView, 0);
        ctx.lineTo(curXView, canvasW);

        // Minor marks
        for (var i = 1; i < numTicksPerMajor; ++i) {
          var xView = Math.floor(curXView + minorMarkDistancePx * i);
          ctx.moveTo(xView, canvasH - minorTickH);
          ctx.lineTo(xView, canvasH);
        }

        ctx.stroke();
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
      // Does nothing. There's nothing interesting to pick on the viewport
      // track.
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
      // Does nothing. There's nothing interesting to pick on the viewport
      // track.
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
      var skipDistancePix = 1;
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
    TimelineThreadTrack: TimelineThreadTrack,
    TimelineViewportTrack: TimelineViewportTrack,
    TimelineCpuTrack: TimelineCpuTrack
  };
});
