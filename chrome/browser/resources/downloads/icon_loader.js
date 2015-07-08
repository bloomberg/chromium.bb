// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  var IconLoader = {
    /** @private {boolean} */
    isIconLoading_: false,

    /** @private {Array<{img: HTMLImageElement, url: string}>} */
    iconsToLoad_: [],

    /**
     * Load the provided |url| into |img.src| after appending ?scale=.
     * @param {!HTMLImageElement} img An <img> to show the loaded image in.
     * @param {string} url A remote image URL to load.
     */
    loadScaledIcon: function(img, url) {
      var scale = '?scale=' + window.devicePixelRatio + 'x';
      this.iconsToLoad_.push({img: img, url: url + scale});
      this.loadNextIcon_();
    },

    /** @private */
    loadNextIcon_: function() {
      if (this.isIconLoading_)
        return;

      this.isIconLoading_ = true;

      while (this.iconsToLoad_.length) {
        var request = IconLoader.iconsToLoad_.shift();
        var img = request.img;

        if (img.src == request.url)
          continue;

        img.onabort = img.onerror = img.onload = function() {
          this.isIconLoading_ = false;
          this.loadNextIcon_();
        }.bind(this);

        img.src = request.url;
        return;
      }

      // If we reached here, there's no more work to do.
      this.isIconLoading_ = false;
    },
  };

  return {IconLoader: IconLoader};
});
