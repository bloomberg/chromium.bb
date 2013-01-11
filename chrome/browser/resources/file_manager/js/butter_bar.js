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
 * @param {MetadataCache} metadataCache The metadata cache.
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

  this.copyManager_.addEventListener('copy-progress',
                                     this.onCopyProgress_.bind(this));
  this.copyManager_.addEventListener('delete',
                                     this.onDelete_.bind(this));
}

/**
 * Name of action which should be displayed as an 'x' button instead of
 * link with text.
 */
ButterBar.ACTION_X = '--action--x--';

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

  var butterMessage = this.butter_.querySelector('.butter-message');
   butterMessage.textContent = message;
  if (message && !this.isVisible_()) {
    // The butter bar is made visible on the first non-empty message.
    this.butter_.classList.add('visible');
    this.lastShowTime_ = Date.now();
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
 * @private
 * @return {string?} The type of operation.
 */
ButterBar.prototype.transferType_ = function() {
  var progress = this.progress_;
  if (!progress ||
      progress.pendingMoves === 0 && progress.pendingCopies === 0 &&
      progress.pendingZips === 0)
    return 'TRANSFER';

  if (progress.pendingZips > 0) {
    return 'ZIP';
  }

  if (progress.pendingMoves > 0) {
    if (progress.pendingCopies > 0)
      return 'TRANSFER';
    return 'MOVE';
  }

  return 'COPY';
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

  if (this.isVisible_()) {
    this.update_(progressString, options);
  } else {
    options.actions[ButterBar.ACTION_X] =
        this.copyManager_.requestCancel.bind(this.copyManager_);
    this.show(progressString, options);
  }
};

/**
 * 'copy-progress' event handler. Show progress or an appropriate message.
 * @private
 * @param {cr.Event} event A 'copy-progress' event from FileCopyManager.
 */
ButterBar.prototype.onCopyProgress_ = function(event) {
  // Delete operation has higher priority.
  if (this.deleteTaskId_) return;

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
      this.show(str(this.transferType_() + '_CANCELLED'), {timeout: 1000});
      break;

    case 'ERROR':
      if (event.error.reason === 'TARGET_EXISTS') {
        var name = event.error.data.name;
        if (event.error.data.isDirectory)
          name += '/';
        this.showError_(strf(this.transferType_() +
                             '_TARGET_EXISTS_ERROR', name));
      } else if (event.error.reason === 'FILESYSTEM_ERROR') {
        if (event.error.data.toGDrive &&
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
 * Forces the delete task, if any.
 * @return {boolean} Whether the delete task was scheduled.
 * @private
 */
ButterBar.prototype.forceDelete_ = function() {
  if (this.deleteTaskId_) {
    this.copyManager_.forceDeleteTask(this.deleteTaskId_);
    this.deleteTaskId_ = null;
    return true;
  }
  return false;
};

/**
 * Informs user that files were deleted with an undo option.
 * In fact, files will be really deleted after timeout.
 * @param {Array.<Entry>} entries The entries to delete.
 */
ButterBar.prototype.initiateDelete = function(entries) {
  this.forceDelete_();

  var callback = function(id) {
    if (this.deleteTaskId_)
      console.error('Already got a deleteTaskId');
    this.deleteTaskId_ = id;
  }.bind(this);

  this.copyManager_.deleteEntries(entries, callback);
};

/**
 * Hides the delete butter bar, and so forces the delete operation.
 * We can safely force delete, since user has no access to undo button anymore.
 * @return {boolean} Whether there was a delete task.
 */
ButterBar.prototype.forceDeleteAndHide = function() {
  var result = this.forceDelete_();
  if (result) this.hide_();
  return result;
};

/**
 * 'delete' event handler. Shows information about deleting files.
 * @private
 * @param {cr.Event} event A 'delete' event from FileCopyManager.
 */
ButterBar.prototype.onDelete_ = function(event) {
  if (event.id != this.deleteTaskId_) return;

  switch (event.reason) {
    case 'SCHEDULED':
      var props = [];
      for (var i = 0; i < event.urls.length; i++) {
        props[i] = {deleted: true};
      }
      this.metadataCache_.set(event.urls, 'internal', props);

      var title = strf('DELETED_MESSAGE_PLURAL', event.urls.length);
      if (event.urls.length == 1) {
        var fullPath = util.extractFilePath(event.urls[0]);
        var fileName = PathUtil.split(fullPath).pop();
        title = strf('DELETED_MESSAGE', fileName);
      }

      var actions = {};
      actions[str('UNDO_DELETE')] = this.undoDelete_.bind(this);
      this.show(title, { actions: actions, timeout: 0 });
      break;

    case 'CANCELLED':
    case 'SUCCESS':
      var props = [];
      for (var i = 0; i < event.urls.length; i++) {
        props[i] = {deleted: false};
      }
      this.metadataCache_.set(event.urls, 'internal', props);

      this.deleteTaskId_ = null;
      this.hide_();
      break;

    default:
      console.log('Unknown "delete" event reason: ' + event.reason);
  }
};

/**
 * Undo the delete operation.
 * @private
 */
ButterBar.prototype.undoDelete_ = function() {
  this.copyManager_.cancelDeleteTask(this.deleteTaskId_);
};
