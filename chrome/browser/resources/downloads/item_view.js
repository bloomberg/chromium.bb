// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  /**
   * Creates and updates the DOM representation for a download.
   * @param {!downloads.ThrottledIconLoader} iconLoader
   * @constructor
   */
  function ItemView(iconLoader) {
    /** @private {!downloads.ThrottledIconLoader} */
    this.iconLoader_ = iconLoader;

    this.node = $('templates').querySelector('.download').cloneNode(true);

    this.safe_ = this.queryRequired_('.safe');
    this.since_ = this.queryRequired_('.since');
    this.dateContainer = this.queryRequired_('.date-container');
    this.date_ = this.queryRequired_('.date');
    this.save_ = this.queryRequired_('.save');
    this.backgroundProgress_ = this.queryRequired_('.progress.background');
    this.foregroundProgress_ = /** @type !HTMLCanvasElement */(
        this.queryRequired_('canvas.progress'));
    this.safeImg_ = /** @type !HTMLImageElement */(
        this.queryRequired_('.safe img'));
    this.fileName_ = this.queryRequired_('span.name');
    this.fileLink_ = this.queryRequired_('[is="action-link"].name');
    this.status_ = this.queryRequired_('.status');
    this.srcUrl_ = this.queryRequired_('.src-url');
    this.show_ = this.queryRequired_('.show');
    this.retry_ = this.queryRequired_('.retry');
    this.pause_ = this.queryRequired_('.pause');
    this.resume_ = this.queryRequired_('.resume');
    this.safeRemove_ = this.queryRequired_('.safe .remove');
    this.cancel_ = this.queryRequired_('.cancel');
    this.controlledBy_ = this.queryRequired_('.controlled-by');

    this.dangerous_ = this.queryRequired_('.dangerous');
    this.dangerImg_ = /** @type {!HTMLImageElement} */(
        this.queryRequired_('.dangerous img'));
    this.description_ = this.queryRequired_('.description');
    this.malwareControls_ = this.queryRequired_('.dangerous .controls');
    this.restore_ = this.queryRequired_('.restore');
    this.dangerRemove_ = this.queryRequired_('.dangerous .remove');
    this.save_ = this.queryRequired_('.save');
    this.discard_ = this.queryRequired_('.discard');

    // Event handlers (bound once on creation).
    this.safe_.ondragstart = this.onSafeDragstart_.bind(this);
    this.fileLink_.onclick = this.onFileLinkClick_.bind(this);
    this.show_.onclick = this.onShowClick_.bind(this);
    this.pause_.onclick = this.onPauseClick_.bind(this);
    this.resume_.onclick = this.onResumeClick_.bind(this);
    this.safeRemove_.onclick = this.onSafeRemoveClick_.bind(this);
    this.cancel_.onclick = this.onCancelClick_.bind(this);
    this.restore_.onclick = this.onRestoreClick_.bind(this);
    this.save_.onclick = this.onSaveClick_.bind(this);
    this.dangerRemove_.onclick = this.onDangerRemoveClick_.bind(this);
    this.discard_.onclick = this.onDiscardClick_.bind(this);
  }

  /** Progress meter constants. */
  ItemView.Progress = {
    /** @const {number} */
    START_ANGLE: -0.5 * Math.PI,
    /** @const {number} */
    SIDE: 48,
  };

  /** @const {number} */
  ItemView.Progress.HALF = ItemView.Progress.SIDE / 2;

  ItemView.computeDownloadProgress = function() {
    /**
     * @param {number} a Some float.
     * @param {number} b Some float.
     * @param {number=} opt_pct Percent of min(a,b).
     * @return {boolean} true if a is within opt_pct percent of b.
     */
    function floatEq(a, b, opt_pct) {
      return Math.abs(a - b) < (Math.min(a, b) * (opt_pct || 1.0) / 100.0);
    }

    if (floatEq(ItemView.Progress.scale, window.devicePixelRatio)) {
      // Zooming in or out multiple times then typing Ctrl+0 resets the zoom
      // level directly to 1x, which fires the matchMedia event multiple times.
      return;
    }
    var Progress = ItemView.Progress;
    Progress.scale = window.devicePixelRatio;
    Progress.width = Progress.SIDE * Progress.scale;
    Progress.height = Progress.SIDE * Progress.scale;
    Progress.radius = Progress.HALF * Progress.scale;
    Progress.centerX = Progress.HALF * Progress.scale;
    Progress.centerY = Progress.HALF * Progress.scale;
  };
  ItemView.computeDownloadProgress();

  // Listens for when device-pixel-ratio changes between any zoom level.
  [0.3, 0.4, 0.6, 0.7, 0.8, 0.95, 1.05, 1.2, 1.4, 1.6, 1.9, 2.2, 2.7, 3.5, 4.5].
      forEach(function(scale) {
    var media = '(-webkit-min-device-pixel-ratio:' + scale + ')';
    window.matchMedia(media).addListener(ItemView.computeDownloadProgress);
  });

  /**
   * @return {!HTMLImageElement} The correct <img> to show when an item is
   *     progressing in the foreground.
   */
  ItemView.getForegroundProgressImage = function() {
    var x = window.devicePixelRatio >= 2 ? '2x' : '1x';
    ItemView.foregroundImages_ = ItemView.foregroundImages_ || {};
    if (!ItemView.foregroundImages_[x]) {
      ItemView.foregroundImages_[x] = new Image;
      var IMAGE_URL = 'chrome://theme/IDR_DOWNLOAD_PROGRESS_FOREGROUND_32';
      ItemView.foregroundImages_[x].src = IMAGE_URL + '@' + x;
    }
    return ItemView.foregroundImages_[x];
  };

  ItemView.prototype = {
    /** @param {!downloads.Data} data */
    update: function(data) {
      assert(!this.id_ || data.id == this.id_);
      this.id_ = data.id;  // This is the only thing saved from |data|.

      this.node.classList.toggle('otr', data.otr);

      var dangerText = this.getDangerText_(data);
      this.dangerous_.hidden = !dangerText;
      this.safe_.hidden = !!dangerText;

      this.ensureTextIs_(this.since_, data.since_string);
      this.ensureTextIs_(this.date_, data.date_string);

      if (dangerText) {
        this.ensureTextIs_(this.description_, dangerText);

        var dangerType = data.danger_type;
        var dangerousFile = dangerType == downloads.DangerType.DANGEROUS_FILE;
        this.description_.classList.toggle('malware', !dangerousFile);

        var idr = dangerousFile ? 'IDR_WARNING' : 'IDR_SAFEBROWSING_WARNING';
        var iconUrl = 'chrome://theme/' + idr;
        this.iconLoader_.loadScaledIcon(this.dangerImg_, iconUrl);

        var showMalwareControls =
            dangerType == downloads.DangerType.DANGEROUS_CONTENT ||
            dangerType == downloads.DangerType.DANGEROUS_HOST ||
            dangerType == downloads.DangerType.DANGEROUS_URL ||
            dangerType == downloads.DangerType.POTENTIALLY_UNWANTED;

        this.malwareControls_.hidden = !showMalwareControls;
        this.discard_.hidden = showMalwareControls;
        this.save_.hidden = showMalwareControls;
      } else {
        var iconUrl = 'chrome://fileicon/' + encodeURIComponent(data.file_path);
        this.iconLoader_.loadScaledIcon(this.safeImg_, iconUrl);

        /** @const */ var isInProgress =
            data.state == downloads.States.IN_PROGRESS;
        this.node.classList.toggle('in-progress', isInProgress);

        /** @const */ var completelyOnDisk =
            data.state == downloads.States.COMPLETE &&
            !data.file_externally_removed;

        this.fileLink_.href = data.url;
        this.ensureTextIs_(this.fileLink_, data.file_name);
        this.fileLink_.hidden = !completelyOnDisk;

        /** @const */ var isInterrupted =
            data.state == downloads.States.INTERRUPTED;
        this.fileName_.classList.toggle('interrupted', isInterrupted);
        this.ensureTextIs_(this.fileName_, data.file_name);
        this.fileName_.hidden = completelyOnDisk;

        this.show_.hidden = !completelyOnDisk;

        this.retry_.href = data.url;
        this.retry_.hidden = !data.retry;

        this.pause_.hidden = !isInProgress;

        this.resume_.hidden = !data.resume;

        /** @const */ var isPaused = data.state == downloads.States.PAUSED;
        /** @const */ var showCancel = isPaused || isInProgress;
        this.cancel_.hidden = !showCancel;

        this.safeRemove_.hidden = showCancel ||
            !loadTimeData.getBoolean('allowDeletingHistory');

        /** @const */ var controlledByExtension = data.by_ext_id &&
                                                  data.by_ext_name;
        this.controlledBy_.hidden = !controlledByExtension;
        if (controlledByExtension) {
          var link = this.controlledBy_.querySelector('a');
          link.href = 'chrome://extensions#' + data.by_ext_id;
          link.setAttribute('focus-type', 'controlled-by');
          link.textContent = data.by_ext_name;
        }

        this.ensureTextIs_(this.srcUrl_, data.url);
        this.srcUrl_.href = data.url;
        this.ensureTextIs_(this.status_, this.getStatusText_(data));

        this.foregroundProgress_.hidden = !isInProgress;
        this.backgroundProgress_.hidden = !isInProgress;

        if (isInProgress) {
          this.foregroundProgress_.width = ItemView.Progress.width;
          this.foregroundProgress_.height = ItemView.Progress.height;

          if (!this.progressContext_) {
            /** @private */
            this.progressContext_ = /** @type !CanvasRenderingContext2D */(
                this.foregroundProgress_.getContext('2d'));
          }

          var foregroundImage = ItemView.getForegroundProgressImage();

          // Draw a pie-slice for the progress.
          this.progressContext_.globalCompositeOperation = 'copy';
          this.progressContext_.drawImage(
              foregroundImage,
              0, 0,  // sx, sy
              foregroundImage.width,
              foregroundImage.height,
              0, 0,  // x, y
              ItemView.Progress.width, ItemView.Progress.height);

          this.progressContext_.globalCompositeOperation = 'destination-in';
          this.progressContext_.beginPath();
          this.progressContext_.moveTo(ItemView.Progress.centerX,
                                       ItemView.Progress.centerY);

          // Draw an arc CW for both RTL and LTR. http://crbug.com/13215
          this.progressContext_.arc(
              ItemView.Progress.centerX,
              ItemView.Progress.centerY,
              ItemView.Progress.radius,
              ItemView.Progress.START_ANGLE,
              ItemView.Progress.START_ANGLE + Math.PI * 0.02 * data.percent,
              false);

          this.progressContext_.lineTo(ItemView.Progress.centerX,
                                       ItemView.Progress.centerY);
          this.progressContext_.fill();
          this.progressContext_.closePath();
        }
      }
    },

    destroy: function() {
      if (this.node.parentNode)
        this.node.parentNode.removeChild(this.node);
    },

    /**
     * @param {string} selector A CSS selector (e.g. '.class-name').
     * @return {!Element} The element found by querying for |selector|.
     * @private
     */
    queryRequired_: function(selector) {
      return assert(this.node.querySelector(selector));
    },

    /**
     * Overwrite |el|'s textContent if it differs from |text|.
     * @param {!Element} el
     * @param {string} text
     * @private
     */
    ensureTextIs_: function(el, text) {
      if (el.textContent != text)
        el.textContent = text;
    },

    /**
     * @param {!downloads.Data} data
     * @return {string} Text describing the danger of a download. Empty if not
     *     dangerous.
     */
    getDangerText_: function(data) {
      switch (data.danger_type) {
        case downloads.DangerType.DANGEROUS_FILE:
          return loadTimeData.getStringF('dangerFileDesc', data.file_name);
        case downloads.DangerType.DANGEROUS_URL:
          return loadTimeData.getString('dangerUrlDesc');
        case downloads.DangerType.DANGEROUS_CONTENT:  // Fall through.
        case downloads.DangerType.DANGEROUS_HOST:
          return loadTimeData.getStringF('dangerContentDesc', data.file_name);
        case downloads.DangerType.UNCOMMON_CONTENT:
          return loadTimeData.getStringF('dangerUncommonDesc', data.file_name);
        case downloads.DangerType.POTENTIALLY_UNWANTED:
          return loadTimeData.getStringF('dangerSettingsDesc', data.file_name);
        default:
          return '';
      }
    },

    /**
     * @param {!downloads.Data} data
     * @return {string} User-visible status update text.
     * @private
     */
    getStatusText_: function(data) {
      switch (data.state) {
        case downloads.States.IN_PROGRESS:
        case downloads.States.PAUSED:  // Fallthrough.
          assert(typeof data.progress_status_text == 'string');
          return data.progress_status_text;
        case downloads.States.CANCELLED:
          return loadTimeData.getString('statusCancelled');
        case downloads.States.DANGEROUS:
          break;  // Intentionally hit assertNotReached(); at bottom.
        case downloads.States.INTERRUPTED:
          assert(typeof data.last_reason_text == 'string');
          return data.last_reason_text;
        case downloads.States.COMPLETE:
          return data.file_externally_removed ?
              loadTimeData.getString('statusRemoved') : '';
      }
      assertNotReached();
      return '';
    },

    /**
     * @private
     * @param {Event} e
     */
    onSafeDragstart_: function(e) {
      e.preventDefault();
      chrome.send('drag', [this.id_]);
    },

    /**
     * @param {Event} e
     * @private
     */
    onFileLinkClick_: function(e) {
      e.preventDefault();
      chrome.send('openFile', [this.id_]);
    },

    /** @private */
    onShowClick_: function() {
      chrome.send('show', [this.id_]);
    },

    /** @private */
    onPauseClick_: function() {
      chrome.send('pause', [this.id_]);
    },

    /** @private */
    onResumeClick_: function() {
      chrome.send('resume', [this.id_]);
    },

    /** @private */
    onSafeRemoveClick_: function() {
      chrome.send('remove', [this.id_]);
    },

    /** @private */
    onCancelClick_: function() {
      chrome.send('cancel', [this.id_]);
    },

    /** @private */
    onRestoreClick_: function() {
      this.onSaveClick_();
    },

    /** @private */
    onSaveClick_: function() {
      chrome.send('saveDangerous', [this.id_]);
    },

    /** @private */
    onDangerRemoveClick_: function() {
      this.onDiscardClick_();
    },

    /** @private */
    onDiscardClick_: function() {
      chrome.send('discardDangerous', [this.id_]);
    },
  };

  return {ItemView: ItemView};
});
