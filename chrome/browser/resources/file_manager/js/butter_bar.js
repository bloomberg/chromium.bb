// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The amount of time, before a butter bar will hide after the last update.
 */
var HIDE_DELAY_TIME_MS = 1000;

/**
 * Butter bar is shown on top of the file list and is used to show the copy
 * progress and other messages.
 * @param {HTMLElement} dialogDom FileManager top-level div.
 * @param {FileCopyManagerWrapper} copyManager The copy manager.
 * @param {MetadataCache} metadataCache The metadata cache.
 * @constructor
 */
function ButterBar(dialogDom, copyManager, metadataCache) {
  this.dialogDom_ = dialogDom;
  this.butter_ = this.dialogDom_.querySelector('#butter-bar');
  this.document_ = this.butter_.ownerDocument;
  this.copyManager_ = copyManager;
  this.metadataCache_ = metadataCache;
  this.hideTimeout_ = null;
  this.showTimeout_ = null;
  this.lastShowTime_ = 0;
  this.deleteTaskId_ = null;
  this.currentMode_ = null;
  this.totalDeleted_ = 0;

  this.copyManager_.addEventListener('copy-progress',
                                     this.onCopyProgress_.bind(this));
  this.copyManager_.addEventListener('delete',
                                     this.onDelete_.bind(this));
}

/**
 * Name of action which should be displayed as an 'x' button instead of
 * link with text.
 * @const
 */
ButterBar.ACTION_X = '--action--x--';

/**
 * Butter bar mode.
 * @const
 */
ButterBar.Mode = {
  COPY: 1,
  DELETE: 2,
  ERROR: 3
};

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
 * @param {ButterBar.Mode} mode Butter bar mode.
 * @param {string} message The message to be shown.
 * @param {Object=} opt_options Options: 'actions', 'progress', 'timeout'.
 */
ButterBar.prototype.show = function(mode, message, opt_options) {
  this.currentMode_ = mode;

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
      if (label == ButterBar.ACTION_X) {
        link.className = 'x';
      } else {
        link.textContent = label;
      }
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
 * @param {string} message Message.
 * @param {Object=} opt_options Same as in show().
 * @private
 */
ButterBar.prototype.showError_ = function(message, opt_options) {
  this.show(ButterBar.Mode.ERROR, message, opt_options);
  this.butter_.classList.add('error');
};

/**
 * Set message and/or progress.
 * @param {string} message Message.
 * @param {Object=} opt_options Same as in show().
 * @private
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

  var butterMessage = this.butter_.querySelector('.butter-message');
   butterMessage.textContent = message;
  if (message && !this.isVisible_()) {
    // The butter bar is made visible on the first non-empty message.
    this.butter_.classList.add('visible');
  }
  if (opt_options && 'progress' in opt_options) {
    butterMessage.classList.add('single-line');
    this.butter_.querySelector('.progress-track').style.width =
        (opt_options.progress * 100) + '%';
  } else {
    butterMessage.classList.remove('single-line');
  }
};

/**
 * Hide butter bar. There might be some delay before hiding so that butter bar
 * would be shown for no less than the minimal time.
 * @param {boolean=} opt_force If true hide immediately, default false.
 * @private
 */
ButterBar.prototype.hide_ = function(opt_force) {
  this.clearHideTimeout_();

  if (!this.isVisible_())
    return;

  var delay = HIDE_DELAY_TIME_MS;
  if (opt_force || delay <= 0) {
    this.currentMode_ = null;
    this.butter_.classList.remove('visible');
    this.butter_.querySelector('.progress-bar').hidden = true;
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

/**
 * @return {string?} The type of operation.
 * @private
 */
ButterBar.prototype.transferType_ = function() {
  var progress = this.progress_;
  if (!progress)
    return 'TRANSFER';

  var pendingTransferTypesCount =
      (progress.pendingMoves === 0 ? 0 : 1) +
      (progress.pendingCopies === 0 ? 0 : 1) +
      (progress.pendingZips === 0 ? 0 : 1);

  if (pendingTransferTypesCount != 1)
    return 'TRANSFER';
  else if (progress.pendingMoves > 0)
    return 'MOVE';
  else if (progress.pendingCopies > 0)
    return 'COPY';
  else
    return 'ZIP';
};

/**
 * Set up butter bar for showing copy progress.
 * @private
 */
ButterBar.prototype.showProgress_ = function() {
  this.progress_ = this.copyManager_.getStatus();
  var options = {progress: this.progress_.percentage, actions: {}, timeout: 0};

  var type = this.transferType_();
  var progressString = (this.progress_.pendingItems === 1) ?
          strf(type + '_FILE_NAME', this.progress_.filename) :
          strf(type + '_ITEMS_REMAINING', this.progress_.pendingItems);

  if (this.currentMode_ == ButterBar.Mode.COPY) {
    this.update_(progressString, options);
  } else {
    options.actions[ButterBar.ACTION_X] =
        this.copyManager_.requestCancel.bind(this.copyManager_);
    this.show(ButterBar.Mode.COPY, progressString, options);
  }
};

/**
 * 'copy-progress' event handler. Show progress or an appropriate message.
 * @param {cr.Event} event A 'copy-progress' event from FileCopyManager.
 * @private
 */
ButterBar.prototype.onCopyProgress_ = function(event) {
  // Delete operation has higher priority.
  if (this.currentMode_ == ButterBar.Mode.DELETE)
    return;

  if (event.reason != 'PROGRESS')
    this.clearShowTimeout_();

  switch (event.reason) {
    case 'BEGIN':
      this.showTimeout_ = setTimeout(function() {
        this.showTimeout_ = null;
        this.showProgress_();
      }.bind(this), 500);
      break;

    case 'PROGRESS':
      this.showProgress_();
      break;

    case 'SUCCESS':
      this.hide_();
      break;

    case 'CANCELLED':
      this.show(ButterBar.Mode.DELETE,
                str(this.transferType_() + '_CANCELLED'), { timeout: 1000 });
      break;

    case 'ERROR':
      this.progress_ = this.copyManager_.getStatus();
      if (event.error.reason === 'TARGET_EXISTS') {
        var name = event.error.data.name;
        if (event.error.data.isDirectory)
          name += '/';
        this.showError_(strf(this.transferType_() +
                             '_TARGET_EXISTS_ERROR', name));
      } else if (event.error.reason === 'FILESYSTEM_ERROR') {
        if (event.error.data.toDrive &&
            event.error.data.code === FileError.QUOTA_EXCEEDED_ERR) {
          // The alert will be shown in FileManager.onCopyProgress_.
          this.hide_();
        } else {
          this.showError_(strf(this.transferType_() + '_FILESYSTEM_ERROR',
                               util.getFileErrorString(event.error.data.code)));
          }
      } else {
        this.showError_(strf(this.transferType_() + '_UNEXPECTED_ERROR',
                             event.error));
      }
      break;

    default:
      console.log('Unknown "copy-progress" event reason: ' + event.reason);
  }
};

/**
 * 'delete' event handler. Shows information about deleting files.
 * @param {cr.Event} event A 'delete' event from FileCopyManager.
 * @private
 */
ButterBar.prototype.onDelete_ = function(event) {
  switch (event.reason) {
    case 'BEGIN':
      if (this.currentMode_ != ButterBar.Mode.DELETE)
        this.totalDeleted_ = 0;
    case 'PROGRESS':
      var props = [];
      for (var i = 0; i < event.urls.length; i++) {
        props[i] = { deleted: true };
      }
      this.metadataCache_.set(event.urls, 'internal', props);

      this.totalDeleted_ += event.urls.length;
      var title = strf('DELETED_MESSAGE_PLURAL', this.totalDeleted_);
      if (this.totalDeleted_ == 1) {
        var fullPath = util.extractFilePath(event.urls[0]);
        var fileName = PathUtil.split(fullPath).pop();
        title = strf('DELETED_MESSAGE', fileName);
      }

      if (this.currentMode_ == ButterBar.Mode.DELETE)
        this.update_(title);
      else
        this.show(ButterBar.Mode.DELETE, title, { timeout: 0 });
      break;

    case 'SUCCESS':
      var props = [];
      for (var i = 0; i < event.urls.length; i++) {
        props[i] = { deleted: false };
      }
      this.metadataCache_.set(event.urls, 'internal', props);

      this.hide_();
      break;

    default:
      console.log('Unknown "delete" event reason: ' + event.reason);
  }
};
