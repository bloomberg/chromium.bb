// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Responsible for showing banners in the file list.
 * @param {DirectoryModel} directoryModel The model.
 * @param {VolumeManager} volumeManager The manager.
 * @param {DOMDocument} document HTML document.
 * @constructor
 */
function FileListBannerController(directoryModel, volumeManager, document) {
  this.directoryModel_ = directoryModel;
  this.volumeManager_ = volumeManager;
  this.document_ = document;
  this.driveEnabled_ = false;

  var board = str('CHROMEOS_RELEASE_BOARD');
  if (!board.match(/^x86-(mario|zgb|alex)/i) && !board.match(/^stout/i))
    this.checkPromoAvailable_();
  else
    this.newWelcome_ = false;

  var handler = this.checkSpaceAndShowBanner_.bind(this);
  this.directoryModel_.addEventListener('scan-completed', handler);
  this.directoryModel_.addEventListener('rescan-completed', handler);
  this.directoryModel_.addEventListener('directory-changed',
      this.onDirectoryChanged_.bind(this));

  this.unmountedPanel_ = this.document_.querySelector('#unmounted-panel');
  this.volumeManager_.addEventListener('gdata-status-changed',
        this.updateGDataUnmountedPanel_.bind(this));

  util.storage.onChanged.addListener(this.onStorageChange_.bind(this));
  this.welcomeHeaderCounter_ = WELCOME_HEADER_COUNTER_LIMIT;
  this.warningDismissedCounter_ = 0;
  util.storage.sync.get([WELCOME_HEADER_COUNTER_KEY, WARNING_DISMISSED_KEY],
                          function(values) {
    this.welcomeHeaderCounter_ =
        parseInt(values[WELCOME_HEADER_COUNTER_KEY]) || 0;
    this.warningDismissedCounter_ =
        parseInt(values[WARNING_DISMISSED_KEY]) || 0;
  }.bind(this));
}

/**
 * FileListBannerController extends cr.EventTarget.
 */
FileListBannerController.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Key in localStorage to keep numer of times the Drive Welcome
 * banner has shown.
 */
var WELCOME_HEADER_COUNTER_KEY = 'gdataWelcomeHeaderCounter';

// If the warning was dismissed before, this key stores the quota value
// (as of the moment of dismissal).
// If the warning was never dismissed or was reset this key stores 0.
var WARNING_DISMISSED_KEY = 'gdriveSpaceWarningDismissed';

/**
 * Maximum times Drive Welcome banner could have shown.
 */
var WELCOME_HEADER_COUNTER_LIMIT = 25;

/**
 * Location of the FAQ about Google Drive.
 */
var GOOGLE_DRIVE_FAQ_URL =
      'https://support.google.com/chromeos/?p=filemanager_drive';

/**
 * Location of the page to buy more storage for Google Drive.
 */
var GOOGLE_DRIVE_BUY_STORAGE =
    'https://www.google.com/settings/storage';

var GOOGLE_DRIVE_REDEEM =
    'http://www.google.com/intl/en/chrome/devices/goodies.html';

/**
 * Location of the FAQ about the downloads directory.
 */
var DOWNLOADS_FAQ_URL =
    'http://support.google.com/chromeos/bin/answer.py?answer=1061547';

/**
 * Location of the help page about connecting to Google Drive.
 */
var GOOGLE_DRIVE_ERROR_HELP_URL =
    'https://support.google.com/chromeos/?p=filemanager_driveerror';

/**
 * @param {number} value How many times the Drive Welcome header banner
 * has shown.
 * @private
 */
FileListBannerController.prototype.setWelcomeHeaderCounter_ = function(value) {
  var values = {};
  values[WELCOME_HEADER_COUNTER_KEY] = value;
  util.storage.local.set(values);
};

/**
 * @param {number} value How many times the low space warning has dismissed.
 * @private
 */
FileListBannerController.prototype.setWarningDismissedCounter_ =
    function(value) {
  var values = {};
  values[WARNING_DISMISSED_KEY] = value;
  util.storage.local.set(values);
};

/**
 * util.storage.onChanged event handler.
 * @param {Object.<string, Object>} changes Changes values.
 * @param {string} areaName "local" or "sync".
 * @private
 */
FileListBannerController.prototype.onStorageChange_ = function(changes,
                                                               areaName) {
  if (areaName == 'local' && WELCOME_HEADER_COUNTER_KEY in changes) {
    this.welcomeHeaderCounter_ = changes[WELCOME_HEADER_COUNTER_KEY].newValue;
  }
  if (areaName == 'local' && WARNING_DISMISSED_KEY in changes) {
    this.warningDismissedCounter_ = changes[WARNING_DISMISSED_KEY].newValue;
  }
};

/**
 * @param {string} type 'none'|'page'|'header'.
 * @param {string} messageId Reource ID of the message.
 * @private
 */
FileListBannerController.prototype.showBanner_ = function(type, messageId) {
  this.showGDataWelcome_(type);

  var container = this.document_.querySelector('.gdrive-welcome.' + type);
  if (container.firstElementChild)
    return;  // Do not re-create.

  if (!this.document_.querySelector('link[gdrive-welcome-style]')) {
    var style = this.document_.createElement('link');
    style.rel = 'stylesheet';
    style.href = 'css/gdrive_welcome.css';
    style.setAttribute('gdrive-welcome-style', '');
    this.document_.head.appendChild(style);
  }

  var wrapper = util.createChild(container, 'gdrive-welcome-wrapper');
  util.createChild(wrapper, 'gdrive-welcome-icon');

  var close = util.createChild(wrapper, 'cr-dialog-close');
  close.addEventListener('click', this.closeBanner_.bind(this));

  var message = util.createChild(wrapper, 'gdrive-welcome-message');

  var title = util.createChild(message, 'gdrive-welcome-title');

  var text = util.createChild(message, 'gdrive-welcome-text');
  text.innerHTML = str(messageId);

  var links = util.createChild(message, 'gdrive-welcome-links');

  var more;
  if (this.newWelcome_) {
    title.textContent = str('DRIVE_WELCOME_TITLE_ALTERNATIVE');
    more = util.createChild(links,
        'gdata-welcome-button gdata-welcome-start', 'a');
    more.textContent = str('DRIVE_WELCOME_GET_STARTED');
    more.href = GOOGLE_DRIVE_REDEEM;
  } else {
    title.textContent = str('DRIVE_WELCOME_TITLE');
    more = util.createChild(links, 'plain-link', 'a');
    more.textContent = str('DRIVE_LEARN_MORE');
    more.href = GOOGLE_DRIVE_FAQ_URL;
  }
  more.target = '_blank';

  var dismiss;
  if (this.newWelcome_)
    dismiss = util.createChild(links, 'gdata-welcome-button');
  else
    dismiss = util.createChild(links, 'plain-link');

  dismiss.classList.add('gdrive-welcome-dismiss');
  dismiss.textContent = str('DRIVE_WELCOME_DISMISS');
  dismiss.addEventListener('click', this.closeBanner_.bind(this));

  this.previousDirWasOnGData_ = false;
};

/**
 * Desides whether to show a banner and if so which one.
 * @private
 */
FileListBannerController.prototype.maybeShowBanner_ = function() {
  if (!this.isOnGData()) {
    this.cleanupGDataWelcome_();
    this.previousDirWasOnGData_ = false;
    return;
  }

  if (this.welcomeHeaderCounter_ >= WELCOME_HEADER_COUNTER_LIMIT ||
      !this.directoryModel_.isDriveMounted())
    return;

  if (this.directoryModel_.getFileList().length == 0 &&
      this.welcomeHeaderCounter_ == 0) {
    // Only show the full page banner if the header banner was never shown.
    // Do not increment the counter.
    // The timeout below is required because sometimes another
    // 'rescan-completed' event arrives shortly with non-empty file list.
    var self = this;
    setTimeout(this.preparePromo_.bind(this, function() {
      var container = self.document_.querySelector('.dialog-container');
      if (self.isOnGData() &&
          self.welcomeHeaderCounter_ == 0) {
        self.showBanner_('page', 'DRIVE_WELCOME_TEXT_LONG');
      }
    }, 2000));
  } else if (this.welcomeHeaderCounter_ < WELCOME_HEADER_COUNTER_LIMIT) {
    // We do not want to increment the counter when the user navigates
    // between different directories on GDrive, but we increment the counter
    // once anyway to prevent the full page banner from showing.
     if (!this.previousDirWasOnGData_ || this.welcomeHeaderCounter_ == 0) {
       var self = this;
       this.setWelcomeHeaderCounter_(this.welcomeHeaderCounter_ + 1);
       this.preparePromo_(function() {
         self.showBanner_('header', 'DRIVE_WELCOME_TEXT_SHORT');
       });
     }
   } else {
     this.closeBanner_();
   }
   this.previousDirWasOnGData_ = true;
};

/**
 * Show or hide the "Low Google Drive space" warning.
 * @param {boolean} show True if the box need to be shown.
 * @param {object} sizeStats Size statistics. Should be defined when showing the
 *     warning.
 * @private
 */
FileListBannerController.prototype.showLowGDriveSpaceWarning_ =
      function(show, sizeStats) {
  var box = this.document_.querySelector('.gdrive-space-warning');

  // Avoid showing two banners.
  // TODO(kaznacheev): Unify the low space warning and the promo header.
  if (show)
    this.cleanupGDataWelcome_();

  if (box.hidden == !show)
    return;

  if (this.warningDismissedCounter_) {
    if (this.warningDismissedCounter_ ==
            sizeStats.totalSizeKB && // Quota had not changed
        sizeStats.remainingSizeKB / sizeStats.totalSizeKB < 0.15) {
      // Since the last dismissal decision the quota has not changed AND
      // the user did not free up significant space. Obey the dismissal.
      show = false;
    } else {
      // Forget the dismissal. Warning will be shown again.
      this.setWarningDismissedCounter_(0);
    }
  }

  box.textContent = '';
  if (show) {
    var icon = this.document_.createElement('div');
    icon.className = 'gdrive-icon';
    box.appendChild(icon);

    var text = this.document_.createElement('div');
    text.className = 'gdrive-text';
    text.textContent = strf('DRIVE_SPACE_AVAILABLE_LONG',
        util.bytesToString(sizeStats.remainingSizeKB * 1024));
    box.appendChild(text);

    var link = this.document_.createElement('a');
    link.className = 'plain-link';
    link.textContent = str('DRIVE_BUY_MORE_SPACE_LINK');
    link.href = GOOGLE_DRIVE_BUY_STORAGE;
    link.target = '_blank';
    box.appendChild(link);

    var close = this.document_.createElement('div');
    close.className = 'cr-dialog-close';
    box.appendChild(close);
    close.addEventListener('click', function(total) {
      window.localStorage[WARNING_DISMISSED_KEY] = total;
      box.hidden = true;
      this.requestRelayout_(100);
    }.bind(this, sizeStats.totalSizeKB));
  }

  if (box.hidden != !show) {
    box.hidden = !show;
    this.requestRelayout_(100);
  }
};

/**
 * Start or stop monitoring free space depending on the new value of current
 * directory path. In case the space is low shows a warning box.
 * @param {string} currentPath New path to the current directory.
 * @private
 */
FileListBannerController.prototype.checkFreeSpace_ = function(currentPath) {
  var self = this;
  var scheduleCheck = function(timeout, root, threshold) {
    if (self.checkFreeSpaceTimer_) {
      clearTimeout(self.checkFreeSpaceTimer_);
      self.checkFreeSpaceTimer_ = null;
    }

    if (timeout) {
      self.checkFreeSpaceTimer_ = setTimeout(
          doCheck, timeout, root, threshold);
    }
  };

  var doCheck = function(root, threshold) {
    // Remember our invocation timer, because getSizeStats is long and
    // asynchronous call.
    var selfTimer = self.checkFreeSpaceTimer_;

    chrome.fileBrowserPrivate.getSizeStats(
        util.makeFilesystemUrl(root),
        function(sizeStats) {
          // If new check started while we were in async getSizeStats call,
          // then we shouldn't do anything.
          if (selfTimer != self.checkFreeSpaceTimer_)
            return;

          // sizeStats is undefined, if some error occurs.
          var ratio = (sizeStats && sizeStats.totalSizeKB > 0) ?
              sizeStats.remainingSizeKB / sizeStats.totalSizeKB : 1;

          var lowDiskSpace = ratio < threshold;

          if (root == RootDirectory.DOWNLOADS)
            self.showLowDownloadsSpaceWarning_(lowDiskSpace);
          else
            self.showLowGDriveSpaceWarning_(lowDiskSpace, sizeStats);

          // If disk space is low, check it more often. User can delete files
          // manually and we should not bother her with warning in this case.
          scheduleCheck(lowDiskSpace ? 1000 * 5 : 1000 * 30, root, threshold);
        });
  };

  // TODO(kaznacheev): Unify the two low space warning.
  var root = PathUtil.getRootPath(currentPath);
  if (root === RootDirectory.DOWNLOADS) {
    scheduleCheck(500, root, 0.2);
    this.showLowGDriveSpaceWarning_(false);
  } else if (root === RootDirectory.GDATA) {
    scheduleCheck(500, root, 0.1);
    this.showLowDownloadsSpaceWarning_(false);
  } else {
    scheduleCheck(0);

    this.showLowDownloadsSpaceWarning_(false);
    this.showLowGDriveSpaceWarning_(false);
  }
};

/**
 * Closes the Drive Welcome banner.
 * @private
 */
FileListBannerController.prototype.closeBanner_ = function() {
  this.cleanupGDataWelcome_();
  // Stop showing the welcome banner.
  this.setWelcomeHeaderCounter_(WELCOME_HEADER_COUNTER_LIMIT);
};

/**
 * Shows or hides the Low Disck Space banner.
 * @private
 */
FileListBannerController.prototype.checkSpaceAndShowBanner_ = function() {
  var self = this;
  this.preparePromo_(function() {
    if (this.newWelcome_ && this.isOnGData()) {
      chrome.fileBrowserPrivate.getSizeStats(
          util.makeFilesystemUrl(this.directoryModel_.getCurrentRootPath()),
          function(result) {
            // Already >= 100 GB.
            if (result && result.totalSizeKB >= 100 * 1024 * 1024)
              self.newWelcome_ = false;
            self.maybeShowBanner_();
          });
    } else {
      self.maybeShowBanner_();
    }
  });
};

/**
 * @return {boolean} True if current directory is on Drive.
 */
FileListBannerController.prototype.isOnGData = function() {
  return this.directoryModel_.getCurrentRootType() === RootType.GDATA;
};

/**
 * Shows the Drive Welcome banner.
 * @param {string} type 'page'|'head'|'none'.
 * @private
 */
FileListBannerController.prototype.showGDataWelcome_ = function(type) {
  var container = this.document_.querySelector('.dialog-container');
  if (container.getAttribute('gdrive-welcome') != type) {
    container.setAttribute('gdrive-welcome', type);
    this.requestRelayout_(200);  // Resize only after the animation is done.
  }
};

/**
 * Update the UI when the current directory changes.
 *
 * @param {cr.Event} event The directory-changed event.
 * @private
 */
FileListBannerController.prototype.onDirectoryChanged_ = function(event) {
  this.checkFreeSpace_(this.directoryModel_.getCurrentDirPath());

  if (!this.isOnGData())
    this.cleanupGDataWelcome_();

  this.updateGDataUnmountedPanel_();
  if (this.isOnGData())
    this.unmountedPanel_.classList.remove('retry-enabled');
};

/**
 * removes the Drive Welcome banner.
 * @private
 */
FileListBannerController.prototype.cleanupGDataWelcome_ = function() {
  this.showGDataWelcome_('none');
};

/**
 * Notifies the file manager what layout must be recalculated.
 * @param {number} delay In milliseconds.
 * @private
 */
FileListBannerController.prototype.requestRelayout_ = function(delay) {
  var self = this;
  setTimeout(function() {
    cr.dispatchSimpleEvent(self, 'relayout');
  }, delay);
};

/**
 * Show or hide the "Low disk space" warning.
 * @param {boolean} show True if the box need to be shown.
 * @private
 */
FileListBannerController.prototype.showLowDownloadsSpaceWarning_ =
    function(show) {
  var box = this.document_.querySelector('.downloads-warning');

  if (box.hidden == !show) return;

  if (show) {
    var html = util.htmlUnescape(str('DOWNLOADS_DIRECTORY_WARNING'));
    box.innerHTML = html;
    var link = box.querySelector('a');
    link.href = DOWNLOADS_FAQ_URL;
    link.target = '_blank';
  } else {
    box.innerHTML = '';
  }

  box.hidden = !show;
  this.requestRelayout_(100);
};

/**
 * Creates contents for the GDATA unmounted panel.
 * @private
 */
FileListBannerController.prototype.ensureGDataUnmountedPanelInitialized_ =
    function() {
  var panel = this.unmountedPanel_;
  if (panel.firstElementChild)
    return;

  function create(parent, tag, className, opt_textContent) {
    var div = panel.ownerDocument.createElement(tag);
    div.className = className;
    div.textContent = opt_textContent || '';
    parent.appendChild(div);
    return div;
  }

  var loading = create(panel, 'div', 'loading', str('DRIVE_LOADING'));
  var spinnerBox = create(loading, 'div', 'spinner-box');
  create(spinnerBox, 'div', 'spinner');
  var progress = create(panel, 'div', 'progress');
  chrome.fileBrowserPrivate.onDocumentFeedFetched.addListener(
      function(fileCount) {
        progress.textContent = strf('DRIVE_LOADING_PROGRESS', fileCount);
      });

  create(panel, 'div', 'error', str('DRIVE_CANNOT_REACH'));

  var retryButton = create(panel, 'button', 'retry', str('DRIVE_RETRY'));
  retryButton.hidden = true;
  var vm = this.volumeManager_;
  retryButton.onclick = function() {
    vm.mountGData(function() {}, function() {});
  };

  var learnMore = create(panel, 'a', 'learn-more plain-link',
                         str('DRIVE_LEARN_MORE'));
  learnMore.href = GOOGLE_DRIVE_ERROR_HELP_URL;
  learnMore.target = '_blank';
};

/**
 * Shows the panel when current directory is GDATA and it's unmounted.
 * Hides it otherwise. The pannel shows spinner if GDATA is mounting or
 * an error message if it failed.
 * @private
 */
FileListBannerController.prototype.updateGDataUnmountedPanel_ = function() {
  var node = this.document_.querySelector('.dialog-container');
  if (this.isOnGData()) {
    var status = this.volumeManager_.getGDataStatus();
    if (status == VolumeManager.GDataStatus.MOUNTING ||
        status == VolumeManager.GDataStatus.ERROR) {
      this.ensureGDataUnmountedPanelInitialized_();
    }
    if (status == VolumeManager.GDataStatus.MOUNTING &&
        this.welcomeHeaderCounter_ == 0) {
      // Do not increment banner counter in order to not prevent the full
      // page banner of being shown (otherwise it would never be shown).
      this.showBanner_('header', 'DRIVE_WELCOME_TEXT_SHORT');
    }
    if (status == VolumeManager.GDataStatus.ERROR)
      this.unmountedPanel_.classList.add('retry-enabled');
    else
      this.unmountedPanel_.classList.remove('retry-enabled');
    node.setAttribute('gdata', status);
  } else {
    node.removeAttribute('gdata');
  }
};

/**
 * Detects what type of promo should be shown.
 * @private
 */
FileListBannerController.prototype.checkPromoAvailable_ = function() {
  this.newWelcome_ = true;
  if (this.promoCallbacks_) {
    for (var i = 0; i < this.promoCallbacks_.length; i++)
      this.promoCallbacks_[i]();
    this.promoCallbacks_ = undefined;
  }
};

/**
 * @param {Function} completeCallback To be called (may be directly) when
 *                                    this.newWelcome_ get ready.
 * @private
 */
FileListBannerController.prototype.preparePromo_ = function(completeCallback) {
  if (this.newWelcome_ !== undefined)
    completeCallback();
  else
    (this.promoCallbacks_ = this.promoCallbacks_ || []).push(completeCallback);
};

