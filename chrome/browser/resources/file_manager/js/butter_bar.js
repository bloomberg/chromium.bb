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
 * Butter bar is shown on top of the file list and is used to show the copy
 * progress and other messages.
 * @constructor
 * @param {HTMLElement} dialogDom FileManager top-level div.
 * @param {FileCopyManagerWrapper} copyManager The copy manager.
 */
function ButterBar(dialogDom, copyManager) {
  this.dialogDom_ = dialogDom;
  this.butter_ = this.dialogDom_.querySelector('#butter-bar');
  this.document_ = this.butter_.ownerDocument;
  this.copyManager_ = copyManager;
  this.hideTimeout_ = null;
  this.showTimeout_ = null;
  this.visible_ = false;
  this.lastShowTime_ = 0;
  this.isError_ = false;

  this.copyManager_.addEventListener('copy-progress',
                                     this.onCopyProgress_.bind(this));
}

/**
 * Show butter bar.
 * @param {string} message The message to be shown.
 * @param {object} opt_options Options: 'actions', 'progress', 'timeout'.
 */
ButterBar.prototype.show = function(message, opt_options) {
  if (opt_options) {
    if ('actions' in opt_options) {
      var actions = this.butter_.querySelector('.actions');
      while (actions.childNodes.length)
        actions.removeChild(actions.firstChild);
      for (var label in opt_options.actions) {
        var link = this.document_.createElement('a');
        link.addEventListener('click', function() {
            opt_options.actions[label]();
            return false;
        });
        actions.appendChild(link);
      }
      actions.classList.remove('hide-in-butter');
    }
    if ('progress' in opt_options) {
      this.butter_.querySelector('.progress-bar').classList.remove(
          'hide-in-butter');
    }
  }

  this.visible_ = true;
  this.isError_ = false;
  this.update_(message, opt_options);
  this.lastShowTime_ = Date.now();
};

/**
 * Show error message in butter bar.
 * @private
 * @param {string} message Message.
 * @param {object} opt_options Same as in show().
 */
ButterBar.prototype.showError_ = function(message, opt_options) {
  this.show(message, opt_options);
  this.isError_ = true;
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

  var timeout = ('timeout' in opt_options) ? opt_options.timeout : 10 * 1000;

  if (this.hideTimeout_)
    clearTimeout(this.hideTimeout_);

  if (timeout) {
    this.hideTimeout_ = setTimeout(function() {
        this.hideButter();
        this.hideTimeout_ = null;
    }.bind(this), timeout);
  }

  this.butter_.querySelector('.butter-message').textContent = message;
  if (message) {
    // The butter bar is made visible on the first non-empty message.
    this.butter_.classList.remove('before-show');
  }
  if (opt_options && 'progress' in opt_options) {
    this.butter_.querySelector('.progress-track').style.width =
        (opt_options.progress * 100) + '%';
  }

  this.butter_.style.left =
      (this.dialogDom_.clientWidth - this.butter_.clientWidth) / 2 + 'px';
};

/**
 * Hide butter bar. There might be some delay before hiding so that butter bar
 * would be shown for no less than the minimal time.
 * @private
 */
ButterBar.prototype.hide_ = function() {
  if (this.visible_) {
    var delay = Math.max(
        MINIMUM_BUTTER_DISPLAY_TIME_MS - (Date.now() - this.lastShowTime_), 0);

    var butter = this.butter_;

    function hideButter() {
      butter.classList.remove('error');
      butter.classList.remove('after-show');
      butter.classList.add('before-show');
      butter.querySelector('.actions').classList.add('hide-in-butter');
      butter.querySelector('.progress-bar').classList.add('hide-in-butter');
    }

    setTimeout(function() { butter.classList.add('after-show'); }, delay);
    setTimeout(hideButter, delay + 1000);
    this.visible_ = false;
  }
};

/**
 * If butter bar shows an error message, close it.
 * @return {boolean} True if butter bar was closed.
 */
ButterBar.prototype.hideError = function() {
  if (this.visible_ && this.isError_) {
    this.hide_();
    clearTimeout(this.hideTimeout_);
    return true;
  } else {
    return false;
  }
};

/**
 * Init butter bar for showing copy progress.
 * @private
 */
ButterBar.prototype.init_ = function() {
  var progress = this.copyManager_.getProgress();
  var options = {progress: progress.percentage, actions: {}, timeout: 0};
  options.actions[str('CANCEL_LABEL')] =
      this.copyManager_.requestCancel.bind(this.copyManager_);
  this.show(strf('PASTE_ITEMS_REMAINING', progress.pendingItems), options);
};

/**
 * 'copy-progress' event handler. Show progress or an appropriate message.
 * @private
 * @param {cr.Event} event A 'copy-progress' event from FileCopyManager.
 */
ButterBar.prototype.onCopyProgress_ = function(event) {
  var progress = this.copyManager_.getProgress();

  switch (event.reason) {
    case 'BEGIN':
      this.hide_();
      clearTimeout(this.timeout_);
      // If the copy process lasts more than 500 ms, we show a progress bar.
      this.showTimeout_ = setTimeout(this.init_.bind(this), 500);
      break;

    case 'PROGRESS':
      if (this.visible_) {
        var options = {'progress': progress.percentage, timeout: 0};
        this.update_(strf('PASTE_ITEMS_REMAINING', progress.pendingItems),
                     options);
      }
      break;

    case 'SUCCESS':
      clearTimeout(this.showTimeout_);
      this.hide_();
      break;

    case 'CANCELLED':
      this.show(str('PASTE_CANCELLED'), {timeout: 1000});
      break;

    case 'ERROR':
      clearTimeout(this.showTimeout_);
      if (event.error.reason === 'TARGET_EXISTS') {
        var name = event.error.data.name;
        if (event.error.data.isDirectory)
          name += '/';
        this.showError_(strf('PASTE_TARGET_EXISTS_ERROR', name));
      } else if (event.error.reason === 'FILESYSTEM_ERROR') {
        if (event.error.data.toGDrive &&
            event.error.data.code === FileError.QUOTA_EXCEEDED_ERR) {
          // The alert will be shown in FileManager.onCopyProgress_.
          this.hide_();
        } else {
          this.showError_(strf('PASTE_FILESYSTEM_ERROR',
                              getFileErrorString(event.error.data.code)));
          }
      } else {
        this.showError_(strf('PASTE_UNEXPECTED_ERROR', event.error));
      }
      break;

    default:
      console.log('Unknown "copy-progress" event reason: ' + event.reason);
  }
};
