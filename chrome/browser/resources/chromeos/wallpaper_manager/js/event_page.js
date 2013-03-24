// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var WALLPAPER_PICKER_WIDTH = 576;
var WALLPAPER_PICKER_HEIGHT = 422;

/**
 * Key to access wallpaper rss in chrome.local.storage.
 */
/** @const */ var AccessRssKey = 'wallpaper-picker-surprise-rss-key';

/**
 * Key to access wallpaper manifest in chrome.local.storage.
 */
/** @const */ var AccessManifestKey = 'wallpaper-picker-manifest-key';

/**
 * Key to access last changed date of a surprise wallpaper.
 */
/** @const */ var AccessLastSurpriseWallpaperChangedDate =
    'wallpaper-last-changed-date-key';

/**
 * Key to access if surprise me feature is enabled or not in
 * chrome.local.storage.
 */
/** @const */ var AccessSurpriseMeEnabledKey = 'surprise-me-enabled-key';

/**
 * Suffix to append to baseURL if requesting high resoultion wallpaper.
 */
/** @const */ var HighResolutionSuffix = '_high_resolution.jpg';

/**
 * URL to get latest wallpaper RSS feed.
 */
/** @const */ var WallpaperRssURL = 'https://commondatastorage.googleapis.' +
    'com/chromeos-wallpaper-public/wallpaper.rss';

/**
 * cros-wallpaper namespace URI.
 */
/** @const */ var WallpaperNameSpaceURI = 'http://commondatastorage.' +
    'googleapis.com/chromeos-wallpaper-public/cros-wallpaper-uri';

var wallpaperPickerWindow;

var surpriseWallpaper = null;

function SurpriseWallpaper() {
  this.storage_ = chrome.storage.local;
}

/**
 * Gets SurpriseWallpaper instance. In case it hasn't been initialized, a new
 * instance is created.
 * @return {SurpriseWallpaper} A SurpriseWallpaper instance.
 */
SurpriseWallpaper.getInstance = function() {
  if (!surpriseWallpaper)
    surpriseWallpaper = new SurpriseWallpaper();
  return surpriseWallpaper;
};

/**
 * Tries to change wallpaper to a new one in the background. May fail due to a
 * network issue.
 */
SurpriseWallpaper.prototype.tryChangeWallpaper = function() {
  var self = this;
  // Try to fetch newest rss as document from server first. If any error occurs,
  // proceed with local copy of rss.
  this.fetchURL_(WallpaperRssURL, 'document', function(xhr) {
    self.saveToLocalStorage(AccessRssKey,
        new XMLSerializer().serializeToString(xhr.responseXML));
    self.updateSurpriseWallpaper(xhr.responseXML);
  }, this.fallbackToLocalRss_.bind(this));
};

/**
 * Downloads resources from url. Calls onSuccess and opt_onFailure accordingly.
 * @param {string} url The url address where we should fetch resources.
 * @param {string} type The response type of XMLHttprequest.
 * @param {function} onSuccess The success callback. It must be called with
 *     current XMLHttprequest object.
 * @param {function=} opt_onFailure The failure callback.
 * @private
 */
SurpriseWallpaper.prototype.fetchURL_ = function(url, type, onSuccess,
                                                 opt_onFailure) {
  var xhr = new XMLHttpRequest();
  var self = this;
  try {
    xhr.addEventListener('loadend', function(e) {
      if (this.status == 200) {
        onSuccess(this);
      } else {
        self.retryLater_();
        if (opt_onFailure)
          opt_onFailure();
      }
    });
    xhr.open('GET', url, true);
    xhr.responseType = type;
    xhr.send(null);
  } catch (e) {
    this.retryLater_();
    if (opt_onFailure)
      opt_onFailure();
  }
};

/**
 * Retries changing the wallpaper 1 hour later. This is called when fetching the
 * rss or wallpaper from server fails.
 * @private
 */
SurpriseWallpaper.prototype.retryLater_ = function() {
  chrome.alarms.create('RetryAlarm', {delayInMinutes: 60});
};

/**
 * Fetches the cached rss feed from local storage in the event of being unable
 * to download the online feed.
 * @private
 */
SurpriseWallpaper.prototype.fallbackToLocalRss_ = function() {
  var self = this;
  this.storage_.get(AccessRssKey, function(items) {
    var rssString = items[AccessRssKey];
    if (rssString) {
      self.updateSurpriseWallpaper(new DOMParser().parseFromString(rssString,
                                                                   'text/xml'));
    } else {
      self.updateSurpriseWallpaper();
    }
  });
};

/**
 * Saves value to local storage that associates with key.
 * @param {string} key The key that associates with value.
 * @param {string} value The value to save to local storage.
 */
SurpriseWallpaper.prototype.saveToLocalStorage = function(key, value) {
  var items = {};
  items[key] = value;
  this.storage_.set(items);
};

/**
 * Starts to change wallpaper. Called after rss is fetched.
 * @param {Document=} opt_rss The fetched rss document. If opt_rss is null, uses
 *     a random wallpaper.
 */
SurpriseWallpaper.prototype.updateSurpriseWallpaper = function(opt_rss) {
  if (opt_rss) {
    var items = opt_rss.querySelectorAll('item');
    var date = new Date(new Date().toDateString()).getTime();
    for (var i = 0; i < items.length; i++) {
      item = items[i];
      var disableDate = new Date(item.getElementsByTagNameNS(
          WallpaperNameSpaceURI, 'disableDate')[0].textContent).getTime();
      var enableDate = new Date(item.getElementsByTagNameNS(
          WallpaperNameSpaceURI, 'enableDate')[0].textContent).getTime();
      var regionsString = item.getElementsByTagNameNS(
          WallpaperNameSpaceURI, 'regions')[0].textContent;
      var regions = regionsString.split(', ');
      if (enableDate <= date && disableDate > date &&
          regions.indexOf(navigator.language) != -1) {
        this.setWallpaperFromRssItem_(item,
                                      function() {},
                                      this.updateRandomWallpaper_.bind(this));
        return;
      }
    }
  }
  // No surprise wallpaper for today at current locale or fetching rss feed
  // fails. Fallback to use a random one from wallpaper server.
  this.updateRandomWallpaper_();
};

/**
 * Sets a new random wallpaper if one has not already been set today.
 * @private
 */
SurpriseWallpaper.prototype.updateRandomWallpaper_ = function() {
  var self = this;
  this.storage_.get(AccessLastSurpriseWallpaperChangedDate, function(items) {
    var dateString = new Date().toDateString();
    // At most one random wallpaper per day.
    if (items[AccessLastSurpriseWallpaperChangedDate] != dateString) {
      self.setRandomWallpaper_(dateString);
    }
  });
};

/**
 * Sets wallpaper to one of the wallpapers displayed in wallpaper picker. If
 * the wallpaper download fails, retry one hour later.
 * @param {string} dateString String representation of current local date.
 * @private
 */
SurpriseWallpaper.prototype.setRandomWallpaper_ = function(dateString) {
  var self = this;
  this.storage_.get(AccessManifestKey, function(items) {
    var manifest = items[AccessManifestKey];
    if (manifest) {
      var index = Math.floor(Math.random() * manifest.wallpaper_list.length);
      var wallpaper = manifest.wallpaper_list[index];
      var wallpaperURL = wallpaper.base_url + HighResolutionSuffix;
      chrome.wallpaperPrivate.setWallpaperIfExists(wallpaperURL,
                                                   wallpaper.default_layout,
                                                   'ONLINE',
                                                   function(exists) {
        if (exists) {
          self.saveToLocalStorage(AccessLastSurpriseWallpaperChangedDate,
                                  dateString);
          return;
        }

        self.fetchURL_(wallpaperURL, 'arraybuffer', function(xhr) {
          if (xhr.response != null) {
            chrome.wallpaperPrivate.setWallpaper(
                xhr.response,
                wallpaper.default_layout,
                wallpaperURL,
                function() {
              self.saveToLocalStorage(AccessLastSurpriseWallpaperChangedDate,
                                      dateString);
            });
          } else {
            self.retryLater_();
          }
        });
      });
    }
  });
};

/**
 * Sets wallpaper to the wallpaper specified by item from rss. If downloading
 * the wallpaper fails, retry one hour later.
 * @param {Element} item The wallpaper rss item element.
 * @param {function} onSuccess Success callback.
 * @param {function} onFailure Failure callback.
 * @private
 */
SurpriseWallpaper.prototype.setWallpaperFromRssItem_ = function(item,
                                                                onSuccess,
                                                                onFailure) {
  var url = item.querySelector('link').textContent;
  var layout = item.getElementsByTagNameNS(
        WallpaperNameSpaceURI, 'layout')[0].textContent;
  var self = this;
  this.fetchURL_(url, 'arraybuffer', function(xhr) {
    if (xhr.response != null) {
      chrome.wallpaperPrivate.setCustomWallpaper(xhr.response, layout, false,
                                                 'surprise_wallpaper',
                                                 onSuccess);
    } else {
      onFailure();
      self.retryLater_();
    }
  }, onFailure);
};

/**
 * Disables the wallpaper surprise me feature.
 * @param {function} onSuccess Success callback.
 * @param {function} onFailure Failure callback.
 */
SurpriseWallpaper.prototype.disableSurpriseMe = function(onSuccess, onFailure) {
  var items = {};
  items[AccessSurpriseMeEnabledKey] = false;
  var self = this;
  this.storage_.set(items, function() {
    if (chrome.runtime.lastError == null) {
      chrome.alarms.clearAll();
      // Makes last changed date invalid.
      self.saveToLocalStorage(AccessLastSurpriseWallpaperChangedDate, '');
      onSuccess();
    } else {
      onFailure();
    }
  });
};

/**
 * Enables the wallpaper surprise me feature. Changes current wallpaper and sets
 * up an alarm to schedule next change around midnight.
 * @param {function} onSuccess Success callback.
 * @param {function} onFailure Failure callback.
 */
SurpriseWallpaper.prototype.enableSurpriseMe = function(onSuccess, onFailure) {
  var self = this;
  var items = {};
  items[AccessSurpriseMeEnabledKey] = true;
  this.storage_.set(items, function() {
    if (chrome.runtime.lastError == null) {
      var nextUpdate = self.nextUpdateTime(new Date());
      chrome.alarms.create({when: nextUpdate});
      self.tryChangeWallpaper();
      onSuccess();
    } else {
      onFailure();
    }
  });
};

/**
 * Calculates when the next wallpaper change should be triggered.
 * @param {Date} now Current time.
 * @return {number} The time when next wallpaper change should happen.
 */
SurpriseWallpaper.prototype.nextUpdateTime = function(now) {
  var nextUpdate = new Date(now.setDate(now.getDate() + 1)).toDateString();
  return new Date(nextUpdate).getTime();
};

chrome.app.runtime.onLaunched.addListener(function() {
  if (wallpaperPickerWindow && !wallpaperPickerWindow.contentWindow.closed) {
    wallpaperPickerWindow.focus();
    chrome.wallpaperPrivate.minimizeInactiveWindows();
    return;
  }

  chrome.app.window.create('main.html', {
    frame: 'none',
    width: WALLPAPER_PICKER_WIDTH,
    height: WALLPAPER_PICKER_HEIGHT,
    minWidth: WALLPAPER_PICKER_WIDTH,
    maxWidth: WALLPAPER_PICKER_WIDTH,
    minHeight: WALLPAPER_PICKER_HEIGHT,
    maxHeight: WALLPAPER_PICKER_HEIGHT,
    transparentBackground: true
  }, function(w) {
    wallpaperPickerWindow = w;
    chrome.wallpaperPrivate.minimizeInactiveWindows();
    w.onClosed.addListener(function() {
      chrome.wallpaperPrivate.restoreMinimizedWindows();
    });
  });
});

chrome.alarms.onAlarm.addListener(function() {
  var nextUpdate = SurpriseWallpaper.getInstance().nextUpdateTime(new Date());
  chrome.alarms.create({when: nextUpdate});
  SurpriseWallpaper.getInstance().tryChangeWallpaper();
});
