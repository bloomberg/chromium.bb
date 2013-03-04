// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Responsible for showing banners in the file list.
 * @param {DirectoryModel} directoryModel The model.
 * @param {VolumeManager} volumeManager The manager.
 * @param {DOMDocument} document HTML document.
 * @param {boolean} showOffers True if we should show offer banners.
 * @constructor
 */
function FileListBannerController(
    directoryModel, volumeManager, document, showOffers) {
  this.directoryModel_ = directoryModel;
  this.volumeManager_ = volumeManager;
  this.document_ = document;
  this.showOffers_ = showOffers;
  this.driveEnabled_ = false;

  if (!util.boardIs('x86-mario') &&
      !util.boardIs('x86-zgb') &&
      !util.boardIs('x86-alex') &&
      !util.boardIs('stout')) {
    this.checkPromoAvailable_();
  } else {
    this.newWelcome_ = false;
  }

  var handler = this.checkSpaceAndShowBanner_.bind(this);
  this.directoryModel_.addEventListener('scan-completed', handler);
  this.directoryModel_.addEventListener('rescan-completed', handler);
  this.directoryModel_.addEventListener('directory-changed',
      this.onDirectoryChanged_.bind(this));

  this.unmountedPanel_ = this.document_.querySelector('#unmounted-panel');
  this.volumeManager_.addEventListener('drive-status-changed',
        this.updateDriveUnmountedPanel_.bind(this));

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
var WELCOME_HEADER_COUNTER_KEY = 'driveWelcomeHeaderCounter';

// If the warning was dismissed before, this key stores the quota value
// (as of the moment of dismissal).
// If the warning was never dismissed or was reset this key stores 0.
var WARNING_DISMISSED_KEY = 'driveSpaceWarningDismissed';

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
  this.showDriveWelcome_(type);

  var container = this.document_.querySelector('.drive-welcome.' + type);
  if (container.firstElementChild)
    return;  // Do not re-create.

  if (!this.document_.querySelector('link[drive-welcome-style]')) {
    var style = this.document_.createElement('link');
    style.rel = 'stylesheet';
    style.href = 'css/drive_welcome.css';
    style.setAttribute('drive-welcome-style', '');
    this.document_.head.appendChild(style);
  }

  var wrapper = util.createChild(container, 'drive-welcome-wrapper');
  util.createChild(wrapper, 'drive-welcome-icon');

  var close = util.createChild(wrapper, 'cr-dialog-close');
  close.addEventListener('click', this.closeBanner_.bind(this));

  var message = util.createChild(wrapper, 'drive-welcome-message');

  var title = util.createChild(message, 'drive-welcome-title');

  var text = util.createChild(message, 'drive-welcome-text');
  text.innerHTML = str(messageId);

  var links = util.createChild(message, 'drive-welcome-links');

  var more;
  if (this.newWelcome_) {
    var welcomeTitle = str('DRIVE_WELCOME_TITLE_ALTERNATIVE');
    if (util.boardIs('link'))
      welcomeTitle = str('DRIVE_WELCOME_TITLE_ALTERNATIVE_1TB');
    title.textContent = welcomeTitle;
    more = util.createChild(links,
        'drive-welcome-button drive-welcome-start', 'a');
    more.textContent = str('DRIVE_WELCOME_CHECK_ELIGIBILITY');
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
    dismiss = util.createChild(links, 'drive-welcome-button');
  else
    dismiss = util.createChild(links, 'plain-link');

  dismiss.classList.add('drive-welcome-dismiss');
  dismiss.textContent = str('DRIVE_WELCOME_DISMISS');
  dismiss.addEventListener('click', this.closeBanner_.bind(this));

  this.previousDirWasOnDrive_ = false;
};

/**
 * Desides whether to show a banner and if so which one.
 * @private
 */
FileListBannerController.prototype.maybeShowBanner_ = function() {
  if (!this.isOnDrive()) {
    this.cleanupDriveWelcome_();
    this.previousDirWasOnDrive_ = false;
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
      if (self.isOnDrive() &&
          self.welcomeHeaderCounter_ == 0) {
        self.showBanner_('page', 'DRIVE_WELCOME_TEXT_LONG');
      }
    }, 2000));
  } else if (this.welcomeHeaderCounter_ < WELCOME_HEADER_COUNTER_LIMIT) {
    // We do not want to increment the counter when the user navigates
    // between different directories on Drive, but we increment the counter
    // once anyway to prevent the full page banner from showing.
     if (!this.previousDirWasOnDrive_ || this.welcomeHeaderCounter_ == 0) {
       var self = this;
       this.setWelcomeHeaderCounter_(this.welcomeHeaderCounter_ + 1);
       this.preparePromo_(function() {
         self.showBanner_('header', 'DRIVE_WELCOME_TEXT_SHORT');
       });
     }
   } else {
     this.closeBanner_();
   }
   this.previousDirWasOnDrive_ = true;
};

/**
 * Show or hide the "Low Google Drive space" warning.
 * @param {boolean} show True if the box need to be shown.
 * @param {Object} sizeStats Size statistics. Should be defined when showing the
 *     warning.
 * @private
 */
FileListBannerController.prototype.showLowDriveSpaceWarning_ =
      function(show, sizeStats) {
  var box = this.document_.querySelector('#volume-space-warning');

  // Avoid showing two banners.
  // TODO(kaznacheev): Unify the low space warning and the promo header.
  if (show)
    this.cleanupDriveWelcome_();

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
    icon.className = 'drive-icon';
    box.appendChild(icon);

    var text = this.document_.createElement('div');
    text.className = 'drive-text';
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
            self.showLowDriveSpaceWarning_(lowDiskSpace, sizeStats);

          // If disk space is low, check it more often. User can delete files
          // manually and we should not bother her with warning in this case.
          scheduleCheck(lowDiskSpace ? 1000 * 5 : 1000 * 30, root, threshold);
        });
  };

  // TODO(kaznacheev): Unify the two low space warning.
  var root = PathUtil.getRootPath(currentPath);
  if (root === RootDirectory.DOWNLOADS) {
    scheduleCheck(500, root, 0.2);
    this.showLowDriveSpaceWarning_(false);
  } else if (root === RootDirectory.DRIVE) {
    scheduleCheck(500, root, 0.1);
    this.showLowDownloadsSpaceWarning_(false);
  } else {
    scheduleCheck(0);

    this.showLowDownloadsSpaceWarning_(false);
    this.showLowDriveSpaceWarning_(false);
  }
};

/**
 * Closes the Drive Welcome banner.
 * @private
 */
FileListBannerController.prototype.closeBanner_ = function() {
  this.cleanupDriveWelcome_();
  // Stop showing the welcome banner.
  this.setWelcomeHeaderCounter_(WELCOME_HEADER_COUNTER_LIMIT);
};

/**
 * Shows or hides the Low Disk Space banner.
 * @private
 */
FileListBannerController.prototype.checkSpaceAndShowBanner_ = function() {
  var self = this;
  this.preparePromo_(function() {
    if (self.newWelcome_ && self.isOnDrive()) {
      chrome.fileBrowserPrivate.getSizeStats(
          util.makeFilesystemUrl(self.directoryModel_.getCurrentRootPath()),
          function(result) {
            var offerSpaceKb = 100 * 1024 * 1024;  // 100GB.
            if (util.boardIs('link'))
              offerSpaceKb = 1024 * 1024 * 1024;  // 1TB.
            if (result && result.totalSizeKB >= offerSpaceKb)
              self.newWelcome_ = false;
            if (!self.showOffers_)
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
FileListBannerController.prototype.isOnDrive = function() {
  return this.directoryModel_.getCurrentRootType() === RootType.DRIVE;
};

/**
 * Shows the Drive Welcome banner.
 * @param {string} type 'page'|'head'|'none'.
 * @private
 */
FileListBannerController.prototype.showDriveWelcome_ = function(type) {
  var container = this.document_.querySelector('.dialog-container');
  if (container.getAttribute('drive-welcome') != type) {
    container.setAttribute('drive-welcome', type);
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

  if (!this.isOnDrive())
    this.cleanupDriveWelcome_();

  this.updateDriveUnmountedPanel_();
  if (this.isOnDrive())
    this.unmountedPanel_.classList.remove('retry-enabled');
};

/**
 * removes the Drive Welcome banner.
 * @private
 */
FileListBannerController.prototype.cleanupDriveWelcome_ = function() {
  this.showDriveWelcome_('none');
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
 * Creates contents for the DRIVE unmounted panel.
 * @private
 */
FileListBannerController.prototype.ensureDriveUnmountedPanelInitialized_ =
    function() {
  var panel = this.unmountedPanel_;
  if (panel.firstElementChild)
    return;

  var create = function(parent, tag, className, opt_textContent) {
    var div = panel.ownerDocument.createElement(tag);
    div.className = className;
    div.textContent = opt_textContent || '';
    parent.appendChild(div);
    return div;
  };

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
    vm.mountDrive(function() {}, function() {});
  };

  var learnMore = create(panel, 'a', 'learn-more plain-link',
                         str('DRIVE_LEARN_MORE'));
  learnMore.href = GOOGLE_DRIVE_ERROR_HELP_URL;
  learnMore.target = '_blank';
};

/**
 * Shows the panel when current directory is DRIVE and it's unmounted.
 * Hides it otherwise. The pannel shows spinner if DRIVE is mounting or
 * an error message if it failed.
 * @private
 */
FileListBannerController.prototype.updateDriveUnmountedPanel_ = function() {
  var node = this.document_.body;
  if (this.isOnDrive()) {
    var status = this.volumeManager_.getDriveStatus();
    if (status == VolumeManager.DriveStatus.MOUNTING ||
        status == VolumeManager.DriveStatus.ERROR) {
      this.ensureDriveUnmountedPanelInitialized_();
    }
    if (status == VolumeManager.DriveStatus.MOUNTING &&
        this.welcomeHeaderCounter_ == 0) {
      // Do not increment banner counter in order to not prevent the full
      // page banner of being shown (otherwise it would never be shown).
      this.showBanner_('header', 'DRIVE_WELCOME_TEXT_SHORT');
    }
    if (status == VolumeManager.DriveStatus.ERROR)
      this.unmountedPanel_.classList.add('retry-enabled');
    else
      this.unmountedPanel_.classList.remove('retry-enabled');
    node.setAttribute('drive', status);
  } else {
    node.removeAttribute('drive');
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
 * @param {function} completeCallback To be called (may be directly) when
 *                                    this.newWelcome_ get ready.
 * @private
 */
FileListBannerController.prototype.preparePromo_ = function(completeCallback) {
  if (this.newWelcome_ !== undefined)
    completeCallback();
  else
    (this.promoCallbacks_ = this.promoCallbacks_ || []).push(completeCallback);
};
