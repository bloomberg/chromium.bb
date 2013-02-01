// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('media', function() {
  'use strict';

  /**
   * This class inherits from <li> and is designed to store and display
   * information about an open media player.
   */
  var MediaPlayer = cr.ui.define('li');

  MediaPlayer.prototype = {
    __proto__: HTMLLIElement.prototype,
    renderer: null,
    id: null,

    /**
     * Decorate this <li> as a MediaPlayer.
     */
    decorate: function() {
      this.properties = {};

      this.url_ = document.createElement('span');
      this.url_.textContent = 'URL Unknown';

      this.summary_ = document.createElement('summary');
      this.summary_.appendChild(this.url_);

      var bufferedDiv = document.createElement('div');
      bufferedDiv.className = 'buffered';
      this.summary_.appendChild(bufferedDiv);

      // Create our canvii.
      function createCanvas(label) {
        var canvas = document.createElement('canvas');
        canvas.width = media.BAR_WIDTH;
        canvas.height = media.BAR_HEIGHT;
        return canvas;
      }
      this.bufferedCanvas_ = createCanvas();
      this.cacheReadsCanvas_ = createCanvas();
      this.cacheWritesCanvas_ = createCanvas();

      // Create our per-canvas entry divs that are initially hidden.
      function addEntry(label, canvas) {
        var labelDiv = document.createElement('div');
        labelDiv.textContent = label;
        var canvasDiv = document.createElement('div');
        canvasDiv.appendChild(canvas);
        var entryDiv = document.createElement('div');
        entryDiv.appendChild(labelDiv);
        entryDiv.appendChild(canvasDiv);
        entryDiv.hidden = true;
        bufferedDiv.appendChild(entryDiv);
        return entryDiv;
      }
      this.bufferedEntry_ = addEntry('Buffered', this.bufferedCanvas_);
      this.cacheReadsEntry_ = addEntry('Cache Reads', this.cacheReadsCanvas_);
      this.cacheWritesEntry_ = addEntry(
          'Cache Writes', this.cacheWritesCanvas_);

      this.details_ = document.createElement('details');
      this.details_.appendChild(this.summary_);

      this.propertyTable_ = document.createElement('table');
      this.events_ = new media.EventList;
      this.metrics_ = new media.Metrics;

      var properties = media.createDetailsLi();
      properties.summary.textContent = 'Properties:';
      properties.details.appendChild(this.propertyTable_);

      var ul = document.createElement('ul');
      ul.appendChild(properties);
      ul.appendChild(this.metrics_);
      ul.appendChild(this.events_);
      this.details_.appendChild(ul);

      this.appendChild(this.details_);
      $('media-players').appendChild(this);
    },

    /**
     * Record an event and update statistics etc.
     * @param {Object} event The event that occurred.
     */
    addEvent: function(event) {
      for (var key in event.params) {
        this.properties[key] = event.params[key];
      }

      if (event.type == 'LOAD' && event.params['url']) {
        this.url_.textContent = event.params['url'];
      }

      if (event.type == 'BUFFERED_EXTENTS_CHANGED') {
        return;
      }
      this.events_.addEvent(event);
      this.metrics_.addEvent(event);
    },

    /**
     * Update the summary line and properties table and redraw the canvas.
     * @return {HTMLElement} A <li> representing this MediaPlayer.
     */
    redraw: function() {
      media.appendDictionaryToTable(this.properties, this.propertyTable_);

      this.setAttribute('status', this.properties.state);

      // Don't bother drawing anything if we don't know the total size.
      var size = this.properties.total_bytes;
      if (!size) {
        return;
      }

      // Draw the state of BufferedResourceLoader.
      this.bufferedEntry_.hidden = false;
      var canvas = this.bufferedCanvas_;
      var context = canvas.getContext('2d');
      context.fillStyle = '#aaa';
      context.fillRect(0, 0, canvas.width, canvas.height);

      var left = this.properties.buffer_start / size * canvas.width;
      var middle = this.properties.buffer_current / size * canvas.width;
      var right = this.properties.buffer_end / size * canvas.width;
      context.fillStyle = '#a0a';
      context.fillRect(left, 0, middle - left, canvas.height);
      context.fillStyle = '#aa0';
      context.fillRect(middle, 0, right - middle, canvas.height);

      // Only show cached file information if we have something.
      var cacheEntry = media.cacheEntriesByKey[this.properties.url];
      if (!cacheEntry) {
        return;
      }

      // Draw cache reads.
      this.cacheReadsEntry_.hidden = false;
      cacheEntry.drawCacheReadsToCanvas(this.cacheReadsCanvas_);

      // Draw cache writes.
      this.cacheWritesEntry_.hidden = false;
      cacheEntry.drawCacheWritesToCanvas(this.cacheWritesCanvas_);
    },
  };

  return {
    MediaPlayer: MediaPlayer
  };
});
