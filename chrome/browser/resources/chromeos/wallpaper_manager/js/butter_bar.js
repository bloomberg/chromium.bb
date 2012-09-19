// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The minimum about of time to display the butter bar for, in ms.
 * Justification is 1000ms for minimum display time plus 300ms for transition
 * duration.
 */
var MINIMUM_BUTTER_DISPLAY_TIME_MS = 1300;

/**
 * Butter bar is shown on top of the wallpaper manager and is used to show the
 * downloading progress and other messages.
 * @constructor
 * @param {HTMLElement} dialogDom Wallpaper manager body tag.
 */
function ButterBar(dialogDom) {
  this.dialogDom_ = dialogDom;
  this.butter_ = this.dialogDom_.querySelector('#butter-bar');
  this.document_ = this.butter_.ownerDocument;
  this.hideTimeout_ = null;
  this.showTimeout_ = null;
  this.xhr_ = null;
  this.lastShowTime_ = 0;
}

// Functions may be reused for general butter bar : ----------------------------

// These functions are copied from butter_bar.js in file manager. We will
// revisit it to see if we can share some code after butter bar is integrated
// with Photo Editor.
// See http://codereview.chromium.org/10916149/ for details.
// TODO(bshe): Remove these functions if we can share code with file manager.

/**
 * @return {boolean} True if visible.
 * @private
 */
ButterBar.prototype.isVisible_ = function() {
  return this.butter_.classList.contains('visible');
};

/**
 * @return {boolean} True if displaying an error.
 * @private
 */
ButterBar.prototype.isError_ = function() {
  return this.butter_.classList.contains('error');
};

/**
 * Show butter bar.
 * @param {string} message The message to be shown.
 * @param {object} opt_options Options: 'actions', 'progress', 'timeout'.
 */
ButterBar.prototype.show = function(message, opt_options) {
  this.clearShowTimeout_();
  this.clearHideTimeout_();

  var actions = this.butter_.querySelector('.actions');
  actions.textContent = '';
  if (opt_options && 'actions' in opt_options) {
    for (var label in opt_options.actions) {
      var link = this.document_.createElement('a');
      link.addEventListener('click', function(callback) {
        callback();
        return false;
      }.bind(null, opt_options.actions[label]));
      actions.appendChild(link);
    }
    actions.hidden = false;
  } else {
    actions.hidden = true;
  }

  this.butter_.querySelector('.progress-bar').hidden =
    !(opt_options && 'progress' in opt_options);

  this.butter_.classList.remove('error');
  this.butter_.classList.remove('visible');  // Will be shown in update_
  this.update_(message, opt_options);
};

/**
 * Show error message in butter bar.
 * @private
 * @param {string} message Message.
 * @param {object} opt_options Same as in show().
 */
ButterBar.prototype.showError_ = function(message, opt_options) {
  this.show(message, opt_options);
  this.butter_.classList.add('error');
};

/**
 * Set message and/or progress.
 * @private
 * @param {string} message Message.
 * @param {object} opt_options Same as in show().
 */
ButterBar.prototype.update_ = function(message, opt_options) {
  if (!opt_options)
    opt_options = {};

  this.clearHideTimeout_();

  var timeout = ('timeout' in opt_options) ? opt_options.timeout : 10 * 1000;
  if (timeout) {
    this.hideTimeout_ = setTimeout(function() {
      this.hideTimeout_ = null;
      this.hide_();
    }.bind(this), timeout);
  }

  this.butter_.querySelector('.butter-message').textContent = message;
  if (message && !this.isVisible_()) {
    // The butter bar is made visible on the first non-empty message.
    this.butter_.classList.add('visible');
    this.lastShowTime_ = Date.now();
  }
  if (opt_options && 'progress' in opt_options) {
    this.butter_.querySelector('.progress-track').style.width =
        (opt_options.progress * 100) + '%';
  }
};

/**
 * Hide butter bar. There might be some delay before hiding so that butter bar
 * would be shown for no less than the minimal time.
 * @param {boolean} opt_force If true hide immediately.
 * @private
 */
ButterBar.prototype.hide_ = function(opt_force) {
  this.clearHideTimeout_();

  if (!this.isVisible_())
    return;

  var delay =
      MINIMUM_BUTTER_DISPLAY_TIME_MS - (Date.now() - this.lastShowTime_);

  if (opt_force || delay <= 0) {
    this.butter_.classList.remove('visible');
  } else {
    // Reschedule hide to comply with the minimal display time.
    this.hideTimeout_ = setTimeout(function() {
      this.hideTimeout_ = null;
      this.hide_(true);
    }.bind(this), delay);
  }
};

/**
 * If butter bar shows an error message, close it.
 * @return {boolean} True if butter bar was closed.
 */
ButterBar.prototype.hideError = function() {
  if (this.isVisible_() && this.isError_()) {
    this.hide_(true /* force */);
    return true;
  } else {
    return false;
  }
};

/**
 * Clear the show timeout if it is set.
 * @private
 */
ButterBar.prototype.clearShowTimeout_ = function() {
  if (this.showTimeout_) {
    clearTimeout(this.showTimeout_);
    this.showTimeout_ = null;
  }
};

/**
 * Clear the hide timeout if it is set.
 * @private
 */
ButterBar.prototype.clearHideTimeout_ = function() {
  if (this.hideTimeout_) {
    clearTimeout(this.hideTimeout_);
    this.hideTimeout_ = null;
  }
};

// Functions specific to WallpaperManager butter bar : -------------------------

/**
 * Sets the XMLHttpRequest object that we want to show progress.
 * @param {XMLHttpRequest} xhr The XMLHttpRequest.
 */
ButterBar.prototype.setRequest = function(xhr) {
  if (this.xhr_)
    this.xhr_.abort();
  this.xhr_ = xhr;
  this.xhr_.addEventListener('loadstart',
                             this.onDownloadStart_.bind(this));
  this.xhr_.addEventListener('progress',
                             this.onDownloadProgress_.bind(this));
  this.xhr_.addEventListener('abort',
                             this.onDownloadAbort_.bind(this));
  this.xhr_.addEventListener('error',
                             this.onDownloadError_.bind(this));
  this.xhr_.addEventListener('load',
                             this.onDownloadComplete_.bind(this));
};

/**
 * Sets the options and strings and shows progress on butter bar.
 * @private
 */
ButterBar.prototype.showProgress_ = function() {
  var options = {progress: this.percentComplete_, actions: {},
                 timeout: 0};

  var progressString = loadTimeData.getString('downloadingLabel');

  if (this.isVisible_()) {
    this.update_(progressString, options);
  } else {
    options.actions['Cancel'] = function() {
      this.xhr_.abort();
    };
    this.show(progressString, options);
  }
};

/**
 * Sets a timeout to show a butter bar when wallpaper downloading starts.
 * @private
 * @param {Event} e A loadstart ProgressEvent from XMLHttpRequest.
 */
ButterBar.prototype.onDownloadStart_ = function(e) {
   this.percentComplete_ = 0;
   this.showTimeout_ = setTimeout(function() {
     this.showTimeout_ = null;
     this.showProgress_();
   }.bind(this), 500);
};

/**
 * Shows abort message in butter bar for 1 second if wallpaper downloading
 * aborted.
 * @private
 * @param {Event} e An abort ProgressEvent from XMLHttpRequest.
 */
ButterBar.prototype.onDownloadAbort_ = function(e) {
   this.show(loadTimeData.getString('downloadCanceled'), {timeout: 1000});
   this.xhr_ = null;
};

/**
 * Hides butter bar when download complete.
 * @private
 * @param {Event} e A load ProgressEvent from XMLHttpRequest.
 */
ButterBar.prototype.onDownloadComplete_ = function(e) {
   this.hide_();
   this.xhr_ = null;
};

/**
 * Shows error message when receiving an error event.
 * @private
 * @param {Event} e An error ProgressEvent from XMLHttpRequest.
 */
ButterBar.prototype.onDownloadError_ = function(e) {
  this.showError_(loadTimeData.getString('downloadFailed'));
  this.xhr_ = null;
};

/**
 * Calculates downloading percentage and shows downloading progress.
 * @private
 * @param {Event} e A progress ProgressEvent from XMLHttpRequest.
 */
ButterBar.prototype.onDownloadProgress_ = function(e) {
  if (e.lengthComputable) {
    this.percentComplete_ = e.loaded / e.total;
  }
  this.showProgress_();
};
