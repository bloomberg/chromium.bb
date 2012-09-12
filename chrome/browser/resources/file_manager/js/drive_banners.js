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
  // It is 'canary' or 'beta' for the other channels.
  var releaseChannel = str('BROWSER_VERSION_MODIFIER') == '';
  this.newWelcome_ = board.match(/^(stumpy|lumpy)/i) && releaseChannel;

  var handler = this.checkSpaceAndShowBanner_.bind(this);
  this.directoryModel_.addEventListener('scan-completed', handler);
  this.directoryModel_.addEventListener('rescan-completed', handler);
  this.directoryModel_.addEventListener('directory-changed',
      this.onDirectoryChanged_.bind(this));

  this.unmountedPanel_ = this.document_.querySelector('#unmounted-panel');
  this.volumeManager_.addEventListener('gdata-status-changed',
        this.updateGDataUnmountedPanel_.bind(this));
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

/**
 * Maximum times Drive Welcome banner could have shown.
 */
var WELCOME_HEADER_COUNTER_LIMIT = 5;

/**
 * Location of the Chromebook information page.
 */
var CHROMEBOOK_INFO_URL = 'http://google.com/chromebook';

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
 * @return {number} How many times the Drive Welcome header banner has shown.
 * @private
 */
FileListBannerController.prototype.getHeaderCounter_ = function() {
  return parseInt(localStorage[WELCOME_HEADER_COUNTER_KEY] || '0');
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
    title.textContent = str('GDATA_WELCOME_TITLE_ALTERNATIVE');
    more = util.createChild(links,
        'gdata-welcome-button gdata-welcome-start', 'a');
    more.textContent = str('GDATA_WELCOME_GET_STARTED');
    more.href = CHROMEBOOK_INFO_URL;
  } else {
    title.textContent = str('GDATA_WELCOME_TITLE');
    more = util.createChild(links, 'plain-link', 'a');
    more.textContent = str('GDATA_LEARN_MORE');
    more.href = GOOGLE_DRIVE_FAQ_URL;
  }

  var dismiss;
  if (this.newWelcome_)
    dismiss = util.createChild(links, 'gdata-welcome-button', 'a');
  else
    dismiss = util.createChild(links, 'plain-link');

  dismiss.classList.add('gdrive-welcome-dismiss');
  dismiss.textContent = str('GDATA_WELCOME_DISMISS');
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

  if (this.getHeaderCounter_() >= WELCOME_HEADER_COUNTER_LIMIT)
    return;

  if (!this.document_.querySelector('link[gdrive-welcome-style]')) {
    var style = this.document_.createElement('link');
    style.rel = 'stylesheet';
    style.href = 'css/gdrive_welcome.css';
    style.setAttribute('gdrive-welcome-style', '');
    this.document_.head.appendChild(style);
    var self = this;
    style.onload = function() { self.maybeShowBanner_() };
    return;
  }

  var counter = this.getHeaderCounter_();

  if (this.directoryModel_.getFileList().length == 0 && counter == 0) {
    // Only show the full page banner if the header banner was never shown.
    // Do not increment the counter.
    // The timeout below is required because sometimes another
    // 'rescan-completed' event arrives shortly with non-empty file list.
    var self = this;
    setTimeout(function() {
      var container = self.document_.querySelector('.dialog-container');
      if (self.isOnGData() &&
          container.getAttribute('gdrive-welcome') != 'header')
            self.showBanner_('page', 'GDATA_WELCOME_TEXT_LONG');
        }, 2000);
  } else if (counter < WELCOME_HEADER_COUNTER_LIMIT) {
    // We do not want to increment the counter when the user navigates
    // between different directories on GDrive, but we increment the counter
    // once anyway to prevent the full page banner from showing.
     if (!this.previousDirWasOnGData_ || counter == 0)
       localStorage[WELCOME_HEADER_COUNTER_KEY] = ++counter;
       this.showBanner_('header', 'GDATA_WELCOME_TEXT_SHORT');
   } else {
     this.closeBanner_();
   }
   this.previousDirWasOnGData_ = true;
};

/**
 * Show or hide the "Low Google Drive space" warning.
 * @param {boolean} show True if the box need to be shown.
 * @param {object} sizeStats Size statistics.
 * @private
 */
FileListBannerController.prototype.showLowGDriveSpaceWarning_ =
      function(show, sizeStats) {
  var box = this.document_.querySelector('.gdrive-space-warning');

  // If the warning was dismissed before, this key stores the quota value
  // (as of the moment of dismissal).
  // If the warning was never dismissed or was reset this key stores 0.
  var WARNING_DISMISSED_KEY = 'gdriveSpaceWarningDismissed';
  var dismissed = parseInt(localStorage[WARNING_DISMISSED_KEY] || '0');

  if (dismissed) {
    if (dismissed == sizeStats.totalSizeKB &&  // Quota had not changed
        sizeStats.remainingSizeKB / sizeStats.totalSizeKB < 0.15) {
      // Since the last dismissal decision the quota has not changed AND
      // the user did not free up significant space. Obey the dismissal.
      show = false;
    } else {
      // Forget the dismissal. Warning will be shown again.
      localStorage[WARNING_DISMISSED_KEY] = 0;
    }
  }

  // Avoid showing two banners.
  // TODO(kaznacheev): Unify the low space warning and the promo header.
  if (show) this.cleanupGDataWelcome_();

  if (box.hidden == !show) return;

  box.textContent = '';
  if (show) {
    var icon = this.document_.createElement('div');
    icon.className = 'gdrive-icon';
    box.appendChild(icon);

    var text = this.document_.createElement('div');
    text.className = 'gdrive-text';
    text.textContent = strf('GDATA_SPACE_AVAILABLE_LONG',
        util.bytesToSi(sizeStats.remainingSizeKB * 1024));
    box.appendChild(text);

    var link = this.document_.createElement('div');
    link.className = 'plain-link';
    link.textContent = str('GDATA_BUY_MORE_SPACE_LINK');
    link.href = GOOGLE_DRIVE_BUY_STORAGE;
    box.appendChild(link);

    var close = this.document_.createElement('div');
    close.className = 'cr-dialog-close';
    box.appendChild(close);
    close.addEventListener('click', function(total) {
      localStorage[WARNING_DISMISSED_KEY] = total;
      box.hidden = true;
      this.requestRelayout_(100);
    }.bind(this, sizeStats.totalSizeKB));
  }

  box.hidden = !show;
  this.requestRelayout_(100);
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
          if (selfTimer != self.checkFreeSpaceTimer_) return;

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
  localStorage[WELCOME_HEADER_COUNTER_KEY] = WELCOME_HEADER_COUNTER_LIMIT;
};

/**
 * Shows or hides the Low Disck Space banner.
 * @private
 */
FileListBannerController.prototype.checkSpaceAndShowBanner_ = function() {
  var self = this;
  if (this.newWelcome_ && this.isOnGData())
    chrome.fileBrowserPrivate.getSizeStats(
        util.makeFilesystemUrl(this.directoryModel_.getCurrentRootPath()),
    function(result) {
      if (result.totalSizeKB >= 100 * 1024 * 1024)  // Already >= 100 GB.
        self.newWelcome_ = false;
        self.maybeShowBanner_();
    });
  else
    self.maybeShowBanner_();
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

  var loading = create(panel, 'div', 'loading', str('GDATA_LOADING'));
  var spinnerBox = create(loading, 'div', 'spinner-box');
  create(spinnerBox, 'div', 'spinner');
  var progress = create(panel, 'div', 'progress');
  chrome.fileBrowserPrivate.onDocumentFeedFetched.addListener(
      function(fileCount) {
        progress.textContent = strf('GDATA_LOADING_PROGRESS', fileCount);
      });

  create(panel, 'div', 'error', str('GDATA_CANNOT_REACH'));

  var retryButton = create(panel, 'button', 'retry', str('GDATA_RETRY'));
  retryButton.hidden = true;
  var vm = this.volumeManager_;
  retryButton.onclick = function() {
    vm.mountGData(function() {}, function() {});
  };

  var learnMore = create(panel, 'a', 'learn-more plain-link',
                         str('GDATA_LEARN_MORE'));
  learnMore.href = GOOGLE_DRIVE_ERROR_HELP_URL;
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
    if (status == VolumeManager.GDataStatus.ERROR)
      this.unmountedPanel_.classList.add('retry-enabled');
    node.setAttribute('gdata', status);
  } else {
    node.removeAttribute('gdata');
  }
};

