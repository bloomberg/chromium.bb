// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var WallpaperUtil = {
  strings: null,  // Object that contains all the flags
  syncFs: null    // syncFileSystem handler
};

/**
 * Run experimental features.
 * @param {function} callback The callback will be executed if 'isExperimental'
 *     flag is set to true.
 */
WallpaperUtil.enabledExperimentalFeatureCallback = function(callback) {
  if (WallpaperUtil.strings) {
    if (WallpaperUtil.strings.isExperimental)
      callback();
  } else {
    chrome.wallpaperPrivate.getStrings(function(strings) {
      WallpaperUtil.strings = strings;
      if (WallpaperUtil.strings.isExperimental)
        callback();
    });
  }
};

/**
 * Request a syncFileSystem handle and run callback on it.
 * @param {function} callback The callback to execute after syncFileSystem
 *     handler is available.
 */
WallpaperUtil.requestSyncFs = function(callback) {
  if (WallpaperUtil.syncFs) {
    callback(WallpaperUtil.syncFs);
  } else {
    chrome.syncFileSystem.requestFileSystem(function(fs) {
      WallpaperUtil.syncFs = fs;
      callback(WallpaperUtil.syncFs);
    });
  }
};

/**
 * Print error to console.error.
 * @param {Event} e The error will be printed to console.error.
 */
// TODO(ranj): Handle different errors differently.
WallpaperUtil.onFileSystemError = function(e) {
  console.error(e);
};

/**
 * Write jpeg/png file data into file entry.
 * @param {FileEntry} fileEntry The file entry that going to be writen.
 * @param {ArrayBuffer} wallpaperData Data for image file.
 * @param {function=} writeCallback The callback that will be executed after
 *     writing data.
 */
WallpaperUtil.writeFile = function(fileEntry, wallpaperData, writeCallback) {
  var uint8arr = new Uint8Array(wallpaperData);
  fileEntry.createWriter(function(fileWriter) {
    var blob = new Blob([new Int8Array(wallpaperData)]);
    fileWriter.write(blob);
    if (writeCallback)
      writeCallback();
  }, WallpaperUtil.onFileSystemError);
};

/**
 * Write jpeg/png file data into syncFileSystem.
 * @param {string} wallpaperFilename The filename that going to be writen.
 * @param {ArrayBuffer} wallpaperData Data for image file.
 * @param {function} onSuccess The callback that will be executed after.
 *     writing data
 */
WallpaperUtil.storePictureToSyncFileSystem = function(
    wallpaperFilename, wallpaperData, onSuccess) {
  var callback = function(fs) {
    fs.root.getFile(wallpaperFilename,
                    {create: false},
                    function() { onSuccess();},  // already exists
                    function(e) {  // not exists, create
                      fs.root.getFile(wallpaperFilename, {create: true},
                                      function(fe) {
                                        WallpaperUtil.writeFile(
                                            fe, wallpaperData, onSuccess);
                                      },
                                      WallpaperUtil.onFileSystemError);
                    });
  };
  WallpaperUtil.requestSyncFs(callback);
};

/**
 * Stores jpeg/png wallpaper into |localDir| in local file system.
 * @param {string} wallpaperFilename File name of wallpaper image.
 * @param {ArrayBuffer} wallpaperData The wallpaper data.
 * @param {string} saveDir The path to store wallpaper in local file system.
 */
WallpaperUtil.storePictureToLocal = function(wallpaperFilename, wallpaperData,
    saveDir) {
  if (!wallpaperData) {
    console.error('wallpaperData is null');
    return;
  }
  var getDirSuccess = function(dirEntry) {
    dirEntry.getFile(wallpaperFilename,
                    {create: false},
                    function() {},  // already exists
                    function(e) {   // not exists, create
                    dirEntry.getFile(wallpaperFilename, {create: true},
                                    function(fe) {
                                      WallpaperUtil.writeFile(fe,
                                                              wallpaperData);
                                    },
                                    WallpaperUtil.onFileSystemError);
                    });
  };
  window.webkitRequestFileSystem(window.PERSISTENT, 1024 * 1024 * 100,
                                 function(fs) {
                                   fs.root.getDirectory(
                                       saveDir, {create: true}, getDirSuccess,
                                       WallpaperUtil.onFileSystemError);
                                 },
                                 WallpaperUtil.onFileSystemError);
};

/**
 * Sets wallpaper from synced file system.
 * @param {string} wallpaperFilename File name used to set wallpaper.
 * @param {string} wallpaperLayout Layout used to set wallpaper.
 * @param {function=} onSuccess Callback if set successfully.
 */
WallpaperUtil.setSyncCustomWallpaper = function(
    wallpaperFilename, wallpaperLayout, onSuccess) {
  var setWallpaperFromSyncCallback = function(fs) {
    if (!wallpaperFilename) {
      console.error('wallpaperFilename is not provided.');
      return;
    }
    if (!wallpaperLayout)
      wallpaperLayout = 'CENTER_CROPPED';
    fs.root.getFile(wallpaperFilename, {create: false}, function(fileEntry) {
      fileEntry.file(function(file) {
        var reader = new FileReader();
        reader.onloadend = function() {
          chrome.wallpaperPrivate.setCustomWallpaper(
              reader.result,
              wallpaperLayout,
              true,
              wallpaperFilename,
              function(thumbnailData) {
                // TODO(ranj): Ignore 'canceledWallpaper' error.
                if (chrome.runtime.lastError) {
                  console.error(chrome.runtime.lastError.message);
                  return;
                }
                WallpaperUtil.storePictureToLocal(wallpaperFilename,
                                                  reader.result, 'original');
                WallpaperUtil.storePictureToLocal(wallpaperFilename,
                                                  reader.result, 'thumbnail');
                if (onSuccess)
                  onSuccess();
              });
        };
        reader.readAsArrayBuffer(file);
      }, WallpaperUtil.onFileSystemError);
    }, function(e) {}  // fail to read file, expected due to download delay
    );
  };
  WallpaperUtil.requestSyncFs(setWallpaperFromSyncCallback);
};

/**
 * Saves value to local storage that associates with key.
 * @param {string} key The key that associates with value.
 * @param {string} value The value to save to local storage.
 * @param {boolen} sync True if the value is saved to sync storage.
 * @param {function=} opt_callback The callback on success, or on failure.
 */
WallpaperUtil.saveToStorage = function(key, value, sync, opt_callback) {
  var items = {};
  items[key] = value;
  if (sync)
    Constants.WallpaperSyncStorage.set(items, opt_callback);
  else
    Constants.WallpaperLocalStorage.set(items, opt_callback);
};

/**
 * Saves user's wallpaper infomation to local and sync storage. Note that local
 * value should be saved first.
 * @param {string} url The url address of wallpaper. For custom wallpaper, it is
 *     the file name.
 * @param {string} layout The wallpaper layout.
 * @param {string} source The wallpaper source.
 */
WallpaperUtil.saveWallpaperInfo = function(url, layout, source) {
  var wallpaperInfo = {
      url: url,
      layout: layout,
      source: source
  };
  WallpaperUtil.saveToStorage(Constants.AccessLocalWallpaperInfoKey,
                              wallpaperInfo, false, function() {
    WallpaperUtil.saveToStorage(Constants.AccessSyncWallpaperInfoKey,
                                wallpaperInfo, true);
  });
};

/**
 * Downloads resources from url. Calls onSuccess and opt_onFailure accordingly.
 * @param {string} url The url address where we should fetch resources.
 * @param {string} type The response type of XMLHttprequest.
 * @param {function} onSuccess The success callback. It must be called with
 *     current XMLHttprequest object.
 * @param {function} onFailure The failure callback.
 * @param {XMLHttpRequest=} opt_xhr The XMLHttpRequest object.
 */
WallpaperUtil.fetchURL = function(url, type, onSuccess, onFailure, opt_xhr) {
  var xhr;
  if (opt_xhr)
    xhr = opt_xhr;
  else
    xhr = new XMLHttpRequest();

  try {
    // Do not use loadend here to handle both success and failure case. It gets
    // complicated with abortion. Unexpected error message may show up. See
    // http://crbug.com/242581.
    xhr.addEventListener('load', function(e) {
      if (this.status == 200) {
        onSuccess(this);
      } else {
        onFailure();
      }
    });
    xhr.addEventListener('error', onFailure);
    xhr.open('GET', url, true);
    xhr.responseType = type;
    xhr.send(null);
  } catch (e) {
    onFailure();
  }
};

/**
 * Sets wallpaper to online wallpaper specified by url and layout
 * @param {string} url The url address where we should fetch resources.
 * @param {string} layout The layout of online wallpaper.
 * @param {function} onSuccess The success callback.
 * @param {function} onFailure The failure callback.
 */
WallpaperUtil.setOnlineWallpaper = function(url, layout, onSuccess, onFailure) {
  var self = this;
  chrome.wallpaperPrivate.setWallpaperIfExists(url, layout, function(exists) {
    if (exists) {
      onSuccess();
      return;
    }

    self.fetchURL(url, 'arraybuffer', function(xhr) {
      if (xhr.response != null) {
        chrome.wallpaperPrivate.setWallpaper(xhr.response, layout, url,
                                             onSuccess);
        self.saveWallpaperInfo(url, layout,
                               Constants.WallpaperSourceEnum.Online);
      } else {
        onFailure();
      }
    }, onFailure);
  });
};

/**
 * Runs chrome.test.sendMessage in test environment. Does nothing if running
 * in production environment.
 *
 * @param {string} message Test message to send.
 */
WallpaperUtil.testSendMessage = function(message) {
  var test = chrome.test || window.top.chrome.test;
  if (test)
    test.sendMessage(message);
};
