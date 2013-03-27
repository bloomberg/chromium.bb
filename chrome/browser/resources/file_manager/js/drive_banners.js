// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Responsible for showing following banners in the file list.
 *  - WelcomeBanner
 *  - AuthFailBanner
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

  this.initializeWelcomeBanner_();
  this.privateOnDirectoryChangedBound_ =
      this.privateOnDirectoryChanged_.bind(this);

  var handler = this.checkSpaceAndMaybeShowWelcomeBanner_.bind(this);
  this.directoryModel_.addEventListener('scan-completed', handler);
  this.directoryModel_.addEventListener('rescan-completed', handler);
  this.directoryModel_.addEventListener('directory-changed',
      this.onDirectoryChanged_.bind(this));

  this.unmountedPanel_ = this.document_.querySelector('#unmounted-panel');
  this.volumeManager_.addEventListener('drive-status-changed',
        this.updateDriveUnmountedPanel_.bind(this));
  this.volumeManager_.addEventListener('drive-connection-changed',
        this.onDriveConnectionChanged_.bind(this));

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

  this.authFailedBanner_ =
      this.document_.querySelector('#drive-auth-failed-warning');
  var authFailedText = this.authFailedBanner_.querySelector('.drive-text');
  authFailedText.innerHTML = util.htmlUnescape(str('DRIVE_NOT_REACHED'));
  authFailedText.querySelector('a').addEventListener('click', function(e) {
    chrome.fileBrowserPrivate.logoutUser();
    e.preventDefault();
  });
  this.maybeShowAuthFailBanner_();
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
 * Initializes the banner to promote DRIVE.
 * This method must be called before any of showing banner functions, and
 * also before registering them as callbacks.
 * @private
 */
FileListBannerController.prototype.initializeWelcomeBanner_ = function() {
  this.useNewWelcomeBanner_ = (!util.boardIs('x86-mario') &&
                               !util.boardIs('x86-zgb') &&
                               !util.boardIs('x86-alex'));
};

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
 * Invoked when the drive connection status is change in the volume manager.
 * @private
 */
FileListBannerController.prototype.onDriveConnectionChanged_ = function() {
  this.maybeShowAuthFailBanner_();
};

/**
 * @param {string} type 'none'|'page'|'header'.
 * @param {string} messageId Reource ID of the message.
 * @private
 */
FileListBannerController.prototype.prepareAndShowWelcomeBanner_ =
    function(type, messageId) {
  this.showWelcomeBanner_(type);

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
  close.addEventListener('click', this.closeWelcomeBanner_.bind(this));

  var message = util.createChild(wrapper, 'drive-welcome-message');

  var title = util.createChild(message, 'drive-welcome-title');

  var text = util.createChild(message, 'drive-welcome-text');
  text.innerHTML = str(messageId);

  var links = util.createChild(message, 'drive-welcome-links');

  var more;
  if (this.useNewWelcomeBanner_) {
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
  if (this.useNewWelcomeBanner_)
    dismiss = util.createChild(links, 'drive-welcome-button');
  else
    dismiss = util.createChild(links, 'plain-link');

  dismiss.classList.add('drive-welcome-dismiss');
  dismiss.textContent = str('DRIVE_WELCOME_DISMISS');
  dismiss.addEventListener('click', this.closeWelcomeBanner_.bind(this));

  this.previousDirWasOnDrive_ = false;
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
    this.cleanupWelcomeBanner_();

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
 * Closes the Drive Welcome banner.
 * @private
 */
FileListBannerController.prototype.closeWelcomeBanner_ = function() {
  this.cleanupWelcomeBanner_();
  // Stop showing the welcome banner.
  this.setWelcomeHeaderCounter_(WELCOME_HEADER_COUNTER_LIMIT);
};

/**
 * Shows or hides the welcome banner for drive.
 * @private
 */
FileListBannerController.prototype.checkSpaceAndMaybeShowWelcomeBanner_ =
    function() {
  if (!this.isOnDrive()) {
    // We are not on the drive file system. Do not show (close) the welcome
    // banner.
    this.cleanupWelcomeBanner_();
    this.previousDirWasOnDrive_ = false;
    return;
  }

  if (this.welcomeHeaderCounter_ >= WELCOME_HEADER_COUNTER_LIMIT ||
      !this.directoryModel_.isDriveMounted()) {
    // The banner is already shown enough times or the drive FS is not mounted.
    // So, do nothing here.
    return;
  }

  if (!this.showOffers_) {
    // Because it is not necessary to show the offer, set
    // |useNewWelcomeBanner_| false here. Note that it probably should be able
    // to do this in the constructor, but there remains non-trivial path,
    // which may be causes |useNewWelcomeBanner_| == true's behavior even
    // if |showOffers_| is false.
    // TODO(hidehiko): Make sure if it is expected or not, and simplify
    // |showOffers_| if possible.
    this.useNewWelcomeBanner_ = false;
  }

  var self = this;
  if (self.useNewWelcomeBanner_) {
    // getSizeStats for Drive file system accesses to the server, so we should
    // minimize the invocation.
    chrome.fileBrowserPrivate.getSizeStats(
        util.makeFilesystemUrl(self.directoryModel_.getCurrentRootPath()),
        function(result) {
          var offerSpaceKb = util.boardIs('link') ?
              1024 * 1024 * 1024 :  // 1TB.
              100 * 1024 * 1024;  // 100GB.
          if (result && result.totalSizeKB >= offerSpaceKb) {
            self.useNewWelcomeBanner_ = false;
          }
          self.maybeShowWelcomeBanner_();
        });
  } else {
    self.maybeShowWelcomeBanner_();
  }
};

/**
 * Decides which banner should be shown, and show it. This method is designed
 * to be called only from checkSpaceAndMaybeShowWelcomeBanner_.
 * @private
 */
FileListBannerController.prototype.maybeShowWelcomeBanner_ = function() {
  if (this.directoryModel_.getFileList().length == 0 &&
      this.welcomeHeaderCounter_ == 0) {
    // Only show the full page banner if the header banner was never shown.
    // Do not increment the counter.
    // The timeout below is required because sometimes another
    // 'rescan-completed' event arrives shortly with non-empty file list.
    setTimeout(function() {
      if (this.isOnDrive() && this.welcomeHeaderCounter_ == 0) {
        this.prepareAndShowWelcomeBanner_('page', 'DRIVE_WELCOME_TEXT_LONG');
      }
    }.bind(this), 2000);
  } else {
    // We do not want to increment the counter when the user navigates
    // between different directories on Drive, but we increment the counter
    // once anyway to prevent the full page banner from showing.
    if (!this.previousDirWasOnDrive_ || this.welcomeHeaderCounter_ == 0) {
      this.setWelcomeHeaderCounter_(this.welcomeHeaderCounter_ + 1);
      this.prepareAndShowWelcomeBanner_('header', 'DRIVE_WELCOME_TEXT_SHORT');
    }
  }
  this.previousDirWasOnDrive_ = true;
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
FileListBannerController.prototype.showWelcomeBanner_ = function(type) {
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
  var root = PathUtil.getTopDirectory(event.newDirEntry.fullPath);
  // Show (or hide) the low space warning.
  this.maybeShowLowSpaceWarning_(root);

  // Add or remove listener to show low space warning, if necessary.
  var isLowSpaceWarningTarget = this.isLowSpaceWarningTarget_(root);
  var previousRoot = PathUtil.getTopDirectory(event.previousDirEntry.fullPath);
  if (isLowSpaceWarningTarget !== this.isLowSpaceWarningTarget_(previousRoot)) {
    if (isLowSpaceWarningTarget) {
      chrome.fileBrowserPrivate.onDirectoryChanged.addListener(
          this.privateOnDirectoryChangedBound_);
    } else {
      chrome.fileBrowserPrivate.onDirectoryChanged.removeListener(
          this.privateOnDirectoryChangedBound_);
    }
  }

  if (!this.isOnDrive()) {
    this.cleanupWelcomeBanner_();
    this.authFailedBanner_.hidden = true;
  }

  this.updateDriveUnmountedPanel_();
  if (this.isOnDrive()) {
    this.unmountedPanel_.classList.remove('retry-enabled');
    this.maybeShowAuthFailBanner_();
  }
};

/**
 * @param {string} root Root directory to be checked.
 * @return {boolean} true if the file system specified by |root| is a target
 *     to show low space warning. Otherwise false.
 * @private
 */
FileListBannerController.prototype.isLowSpaceWarningTarget_ = function(root) {
  return (root == RootDirectory.DOWNLOADS || root == RootDirectory.DRIVE);
};

/**
 * Callback which is invoked when the file system has been changed.
 * @param {Object} event chrome.fileBrowserPrivate.onDirectoryChanged event.
 * @private
 */
FileListBannerController.prototype.privateOnDirectoryChanged_ = function(
    event) {
  var currentRoot = PathUtil.getTopDirectory(
      this.directoryModel_.getCurrentDirPath());
  var eventRoot = PathUtil.getTopDirectory(
      util.extractFilePath(event.directoryUrl));
  if (currentRoot == eventRoot) {
    // The file system we are currently on is changed.
    // So, check the free space.
    this.maybeShowLowSpaceWarning_(eventRoot);
  }
};

/**
 * Shows or hides the low space warning.
 * @param {string} root Root directory of the file system, which we are
 *     interested in.
 * @private
 */
FileListBannerController.prototype.maybeShowLowSpaceWarning_ = function(root) {
  // TODO(kaznacheev): Unify the two low space warning.
  var threshold = 0;
  if (root === RootDirectory.DOWNLOADS) {
    this.showLowDriveSpaceWarning_(false);
    threshold = 0.2;
  } else if (root === RootDirectory.DRIVE) {
    this.showLowDownloadsSpaceWarning_(false);
    threshold = 0.1;
  } else {
    // If the current file system is neither the DOWNLOAD nor the DRIVE,
    // just hide the warning.
    this.showLowDownloadsSpaceWarning_(false);
    this.showLowDriveSpaceWarning_(false);
    return;
  }

  var self = this;
  chrome.fileBrowserPrivate.getSizeStats(
      util.makeFilesystemUrl(root),
      function(sizeStats) {
        var currentRoot = PathUtil.getTopDirectory(
            self.directoryModel_.getCurrentDirPath());
        if (root != currentRoot) {
          // This happens when the current directory is moved during requesting
          // the file system size. Just ignore it.
          return;
        }

        // sizeStats is undefined, if some error occurs.
        var remainingRatio = (sizeStats && sizeStats.totalSizeKB > 0) ?
            (sizeStats.remainingSizeKB / sizeStats.totalSizeKB) : 1;
        var isLowDiskSpace = remainingRatio < threshold;
        if (root == RootDirectory.DOWNLOADS)
          self.showLowDownloadsSpaceWarning_(isLowDiskSpace);
        else
          self.showLowDriveSpaceWarning_(isLowDiskSpace, sizeStats);
      });
};

/**
 * removes the Drive Welcome banner.
 * @private
 */
FileListBannerController.prototype.cleanupWelcomeBanner_ = function() {
  this.showWelcomeBanner_('none');
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
      this.showWelcomeBanner_('header', 'DRIVE_WELCOME_TEXT_SHORT');
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
 * Updates the visibility of Drive Connection Warning banner, retrieving the
 * current connection information.
 * @private
 */
FileListBannerController.prototype.maybeShowAuthFailBanner_ = function() {
  var connection = this.volumeManager_.getDriveConnectionState();
  var showDriveNotReachedMessage =
      connection.type == VolumeManager.DriveConnectionType.OFFLINE &&
      connection.reasons.indexOf('not_ready') !== -1;
  this.authFailedBanner_.hidden = !showDriveNotReachedMessage;
};
