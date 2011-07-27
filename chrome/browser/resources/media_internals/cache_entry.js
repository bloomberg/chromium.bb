// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('media', function() {

  /**
   * The width and height of a CacheEntry canvas in pixels.
   */
  var CANVAS_WIDTH = 500;
  var CANVAS_HEIGHT = 31;

  /**
   * This class represents a file cached by net.
   */
  function CacheEntry() {
    this.read_ = new media.DisjointRangeSet;
    this.written_ = new media.DisjointRangeSet;
    this.available_ = new media.DisjointRangeSet;

    // Set to true when we know the entry is sparse.
    this.sparse = false;
    this.key = null;
    this.size = null;

    // The <details> element representing this CacheEntry.
    this.details_ = document.createElement('details');
    this.details_.className = 'cache-entry';
    this.details_.open = false;

    // The <details> summary line. It contains a chart of requested file ranges
    // and the url if we know it.
    var summary = document.createElement('summary');

    this.summaryText_ = document.createTextNode('');
    summary.appendChild(this.summaryText_);

    summary.appendChild(document.createTextNode(' '));

    // Controls to modify this CacheEntry.
    var controls = document.createElement('span');
    controls.className = 'cache-entry-controls';
    summary.appendChild(controls);
    summary.appendChild(document.createElement('br'));

    // A link to clear recorded data from this CacheEntry.
    var clearControl = document.createElement('a');
    clearControl.href = 'javascript:void(0)';
    clearControl.onclick = this.clear.bind(this);
    clearControl.textContent = '(clear entry)';
    controls.appendChild(clearControl);

    // The canvas upon which to draw the cached sections of a file.
    this.canvas_ = document.createElement('canvas');
    this.canvas_.width = CANVAS_WIDTH;
    this.canvas_.height = CANVAS_HEIGHT;
    summary.appendChild(this.canvas_);

    this.details_.appendChild(summary);

    // A tabular representation of the data in the above canvas.
    this.detailTable_ = document.createElement('table');
    this.detailTable_.className = 'cache-table';
    this.details_.appendChild(this.detailTable_);
  };

  CacheEntry.prototype = {
    /**
     * Mark a range of bytes as read from the cache.
     * @param {int} start The first byte read.
     * @param {int} length The number of bytes read.
     */
    readBytes: function(start, length) {
      start = parseInt(start);
      length = parseInt(length);
      this.read_.add(start, start + length);
      this.available_.add(start, start + length);
      this.sparse = true;
    },

    /**
     * Mark a range of bytes as written to the cache.
     * @param {int} start The first byte written.
     * @param {int} length The number of bytes written.
     */
    writeBytes: function(start, length) {
      start = parseInt(start);
      length = parseInt(length);
      this.written_.add(start, start + length);
      this.available_.add(start, start + length);
      this.sparse = true;
    },

    /**
     * Merge this CacheEntry with another, merging recorded ranges and flags.
     * @param {CacheEntry} other The CacheEntry to merge into this one.
     */
    merge: function(other) {
      this.read_.merge(other.read_);
      this.written_.merge(other.written_);
      this.available_.merge(other.available_);
      this.sparse = this.sparse || other.sparse;
      this.key = this.key || other.key;
      this.size = this.size || other.size;
    },

    /**
     * Clear all recorded ranges from this CacheEntry and redraw this.details_.
     */
    clear: function() {
      this.read_ = new media.DisjointRangeSet;
      this.written_ = new media.DisjointRangeSet;
      this.available_ = new media.DisjointRangeSet;
      this.generateDetails();
    },

    /**
     * Redraw this.canvas_.
     * It should consist of two horizontal bars with highlighted sections to
     * represent which parts of a file have been read from (top) and written to
     * (bottom) the cache.
     * e.g. |xxxxxx----------x|
     *      |-----xxxxx-------|
     */
    generateCanvas: function() {
      var context = this.canvas_.getContext('2d');
      context.textAlign = 'center';
      context.textBaseline = 'middle';

      context.fillStyle = '#aaa';
      context.fillRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);

      if (this.size) {
        var fileSize = this.size;
        drawRange = function(start, end, top) {
          var left = start / fileSize * CANVAS_WIDTH;
          var right = end / fileSize * CANVAS_WIDTH;
          context.fillRect(left, top, right - left, top + CANVAS_HEIGHT / 2);
        };

        context.fillStyle = '#0a0';
        this.read_.map(function(start, end) {
          drawRange(start, end, 0);
        });

        context.fillStyle = '#00a';
        this.written_.map(function(start, end) {
          drawRange(start, end, CANVAS_HEIGHT / 2);
        });

        // Overlay a description of each bar.
        context.fillStyle = '#fff';
        context.fillText('Read from cache.', CANVAS_WIDTH / 2,
                         CANVAS_HEIGHT / 4 - 0.5);
        context.fillText('Written to cache.', CANVAS_WIDTH / 2,
                         CANVAS_HEIGHT * 3 / 4 + 0.5);

        // Add a 1px separator line.
        context.moveTo(0, CANVAS_HEIGHT / 2);
        context.lineTo(CANVAS_WIDTH, CANVAS_HEIGHT / 2);
        context.strokeStyle = '#fff';
        context.stroke();
      } else {
        // We can't draw bars if we don't know how big the file is.
        context.fillStyle = '#fff';
        context.fillText('Unknown file size. See details below.',
                         CANVAS_WIDTH / 2, CANVAS_HEIGHT / 2);
      }
    },

    /**
     * Update this.details_ to contain everything we currently know about
     * this file.
     */
    generateDetails: function() {
      function makeElement(type, content) {
        var element = document.createElement(type);
        element.textContent = content;
        return element;
      };

      this.details_.id = this.key;
      this.summaryText_.textContent = this.key || 'Unknown File';

      this.detailTable_.textContent = '';
      var header = document.createElement('thead');
      var footer = document.createElement('tfoot');
      var body = document.createElement('tbody');
      this.detailTable_.appendChild(header);
      this.detailTable_.appendChild(footer);
      this.detailTable_.appendChild(body);

      var headerRow = document.createElement('tr');
      headerRow.appendChild(makeElement('th', 'Read From Cache'));
      headerRow.appendChild(makeElement('th', 'Written To Cache'));
      header.appendChild(headerRow);

      var footerRow = document.createElement('tr');
      var footerCell = document.createElement('td');
      footerCell.textContent = 'Out of ' + (this.size || 'unkown size');
      footerCell.setAttribute('colspan', 2);
      footerRow.appendChild(footerCell);
      footer.appendChild(footerRow);

      var read = this.read_.map(function(start, end) {
        return start + ' - ' + end;
      });
      var written = this.written_.map(function(start, end) {
        return start + ' - ' + end;
      });

      var length = Math.max(read.length, written.length);
      for (var i = 0; i < length; i++) {
        var row = document.createElement('tr');
        row.appendChild(makeElement('td', read[i] || ''));
        row.appendChild(makeElement('td', written[i] || ''));
        body.appendChild(row);
      }

      this.generateCanvas();
    },

    /**
     * Render this CacheEntry as a <li>.
     * @return {HTMLElement} A <li> representing this CacheEntry.
     */
    toListItem: function() {
      this.generateDetails();

      var result = document.createElement('li');
      result.appendChild(this.details_);
      return result;
    }
  };

  return {
    CacheEntry: CacheEntry
  };
});
