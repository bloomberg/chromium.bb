// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @typedef {{img: HTMLImageElement, url: string}} */
var LoadIconRequest;

cr.define('downloads', function() {
  /**
   * @param {number} maxAllowed The maximum number of simultaneous downloads
   *     allowed.
   * @constructor
   */
  function ThrottledIconLoader(maxAllowed) {
    assert(maxAllowed > 0);

    /** @private {number} */
    this.maxAllowed_ = maxAllowed;

    /** @private {!Array<!LoadIconRequest>} */
    this.requests_ = [];
  }

  ThrottledIconLoader.prototype = {
    /** @private {number} */
    loading_: 0,

    /**
     * Load the provided |url| into |img.src| after appending ?scale=.
     * @param {!HTMLImageElement} img An <img> to show the loaded image in.
     * @param {string} url A remote image URL to load.
     */
    loadScaledIcon: function(img, url) {
      var scaledUrl = url + '?scale=' + window.devicePixelRatio + 'x';
      if (img.src == scaledUrl)
        return;

      this.requests_.push({img: img, url: scaledUrl});
      this.loadNextIcon_();
    },

    /** @private */
    loadNextIcon_: function() {
      if (this.loading_ > this.maxAllowed_ || !this.requests_.length)
        return;

      var request = this.requests_.shift();
      var img = request.img;

      img.onabort = img.onerror = img.onload = function() {
        this.loading_--;
        this.loadNextIcon_();
      }.bind(this);

      this.loading_++;
      img.src = request.url;
    },
  };

  return {ThrottledIconLoader: ThrottledIconLoader};
});
