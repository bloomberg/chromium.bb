// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * VolumeManager is responsible for tracking list of mounted volumes.
 *
 * @constructor
 * @extends {cr.EventTarget}
 */
function VolumeManager() {
  /**
   * The list of archives requested to mount. We will show contents once
   * archive is mounted, but only for mounts from within this filebrowser tab.
   * @type {Object.<string, Object>}
   * @private
   */
  this.requests_ = {};

  /**
   * @type {Object.<string, Object>}
   * @private
   */
  this.mountedVolumes_ = {};

  this.initMountPoints_();
  chrome.fileBrowserPrivate.onMountCompleted.addListener(
        this.onMountCompleted_.bind(this));
  this.gDataStatus_ = VolumeManager.GDataStatus.UNMOUNTED;
}

/**
 * VolumeManager extends cr.EventTarget.
 */
VolumeManager.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * @enum
 */
VolumeManager.Error = {
  /* Internal errors */
  NOT_MOUNTED: 'not_mounted',
  TIMEOUT: 'timeout',

  /* System events */
  UNKNOWN: 'error_unknown',
  INTERNAL: 'error_internal',
  UNKNOWN_FILESYSTEM: 'error_unknown_filesystem',
  UNSUPPORTED_FILESYSTEM: 'error_unsupported_filesystem',
  INVALID_ARCHIVE: 'error_invalid_archive',
  LIBCROS_MISSING: 'error_libcros_missing',
  AUTHENTICATION: 'error_authentication',
  PATH_UNMOUNTED: 'error_path_unmounted'
};

/**
 * @enum
 */
VolumeManager.GDataStatus = {
  UNMOUNTED: 'unmounted',
  MOUNTING: 'mounting',
  ERROR: 'error',
  MOUNTED: 'mounted'
};

/**
 * Time in milliseconds that we wait a respone for. If no response on
 * mount/unmount received the request supposed failed.
 */
VolumeManager.TIMEOUT = 15 * 60 * 1000;

/**
 * Delay in milliseconds GDATA changes its state from |UNMOUNTED| to
 * |MOUNTING|. Used to display progress in the UI.
 */
VolumeManager.MOUNTING_DELAY = 500;

/**
 * @return {VolumeManager} Singleton instance.
 */
VolumeManager.getInstance = function() {
  return VolumeManager.instance_ = VolumeManager.instance_ ||
                                   new VolumeManager();
};

/**
 * @param {VolumeManager.GDataStatus} newStatus New GDATA status.
 * @private
 */
VolumeManager.prototype.setGDataStatus_ = function(newStatus) {
  if (this.gDataStatus_ != newStatus) {
    this.gDataStatus_ = newStatus;
    cr.dispatchSimpleEvent(this, 'gdata-status-changed');
  }
};

/**
 * @return {VolumeManager.GDataStatus} Current GDATA status.
 */
VolumeManager.prototype.getGDataStatus = function() {
  return this.gDataStatus_;
};

/**
 * @param {string} mountPath Volume root path.
 * @return {boolean} True if mounted.
 */
VolumeManager.prototype.isMounted = function(mountPath) {
  this.validateMountPath_(mountPath);
  return mountPath in this.mountedVolumes_;
};

/**
 * Initialized mount points.
 * @private
 */
VolumeManager.prototype.initMountPoints_ = function() {
  var mountedVolumes = [];
  var self = this;
  var index = 0;
  function step(mountPoints) {
    if (index < mountPoints.length) {
      var info = mountPoints[index];
      if (info.mountType == 'gdata')
        console.error('GData is not expected initially mounted');
      var error = info.mountCondition ? 'error_' + info.mountCondition : '';
      function onVolumeInfo(volume) {
        mountedVolumes.push(volume);
        index++;
        step(mountPoints);
      }
      self.makeVolumeInfo_('/' + info.mountPath, error, onVolumeInfo);
    } else {
      for (var i = 0; i < mountedVolumes.length; i++) {
        var volume = mountedVolumes[i];
        self.mountedVolumes_[volume.mountPath] = volume;
      }
      if (mountedVolumes.length > 0)
        cr.dispatchSimpleEvent(self, 'change');
    }
  }

  chrome.fileBrowserPrivate.getMountPoints(step);
};

/**
 * Event handler called when some volume was mounted or unmouted.
 * @param {MountCompletedEvent} event Received event.
 * @private
 */
VolumeManager.prototype.onMountCompleted_ = function(event) {
  if (event.eventType == 'mount') {
    if (event.mountPath) {
      var requestKey = this.makeRequestKey_(
          'mount', event.mountType, event.sourcePath);
      var error = event.status == 'success' ? '' : event.status;
      this.makeVolumeInfo_(event.mountPath, error, function(volume) {
        this.mountedVolumes_[volume.mountPath] = volume;
        this.finishRequest_(requestKey, event.status, event.mountPath);
        cr.dispatchSimpleEvent(this, 'change');
      }.bind(this));
    } else {
      console.log('No mount path');
      this.finishRequest_(requestKey, event.status);
    }
  } else if (event.eventType == 'unmount') {
    var mountPath = event.mountPath;
    this.validateMountPath_(mountPath);
    var status = event.status;
    if (status == VolumeManager.Error.PATH_UNMOUNTED) {
      console.log('Volume already unmounted: ', mountPath);
      status = 'success';
    }
    var requestKey = this.makeRequestKey_('unmount', '', event.mountPath);
    var requested = requestKey in this.requests_;
    if (event.status == 'success' && !requested &&
        mountPath in this.mountedVolumes_) {
      console.log('Mounted volume without a request: ', mountPath);
      var e = new cr.Event('externally-unmounted');
      e.mountPath = mountPath;
      this.dispatchEvent(e);
    }
    this.finishRequest_(requestKey, status);

    if (event.status == 'success') {
      delete this.mountedVolumes_[mountPath];
      cr.dispatchSimpleEvent(this, 'change');
    }
  }

  if (event.mountType == 'gdata') {
    if (event.status == 'success') {
      if (event.eventType == 'mount') {
        this.waitGDataLoaded_(event.mountPath,
            this.setGDataStatus_.bind(this, VolumeManager.GDataStatus.MOUNTED));
      } else if (event.eventType == 'unmount') {
        this.setGDataStatus_(VolumeManager.GDataStatus.UMOUNTED);
      }
    }
  }
};

/**
 * First access to GDrive takes time (to fetch data from the cloud).
 * We want to change state to MOUNTED (likely from MOUNTING) when the
 * drive ready to operate.
 *
 * @param {string} mountPath GData mount path.
 * @param {function()} callback To be called when waiting finish.
 * @private
 */
VolumeManager.prototype.waitGDataLoaded_ = function(mountPath, callback) {
  chrome.fileBrowserPrivate.requestLocalFileSystem(function(filesystem) {
    filesystem.root.getDirectory(mountPath, {}, function() { callback(); });
  });
};

/**
 * @param {string} mountPath Path to the volume.
 * @param {VolumeManager?} error Mounting error if any.
 * @param {function(Object)} callback Result acceptor.
 * @private
 */
VolumeManager.prototype.makeVolumeInfo_ = function(
    mountPath, error, callback) {
  if (error)
    this.validateError_(error);
  this.validateMountPath_(mountPath);
  function onVolumeMetadata(metadata) {
   callback({
     mountPath: mountPath,
     error: error,
     deviceType: metadata && metadata.deviceType,
     readonly: !!metadata && metadata.isReadOnly
   });
  }
  chrome.fileBrowserPrivate.getVolumeMetadata(
      util.makeFilesystemUrl(mountPath), onVolumeMetadata);
};

/**
 * Creates string to match mount events with requests.
 * @param {string} requestType 'mount' | 'unmount'.
 * @param {string} mountType 'device' | 'file' | 'network' | 'gdata'.
 * @param {string} mountOrSourcePath Source path provided by API after
 *     resolving mount request or mountPath for unmount request.
 * @return {string} Key for |this.requests_|.
 * @private
 */
VolumeManager.prototype.makeRequestKey_ = function(requestType,
                                                   mountType,
                                                   mountOrSourcePath) {
  return requestType + ':' + mountType + ':' + mountOrSourcePath;
};


/**
 * @param {Function} successCallback Success callback.
 * @param {Function} errorCallback Error callback.
 */
VolumeManager.prototype.mountGData = function(successCallback, errorCallback) {
  if (this.getGDataStatus() == VolumeManager.GDataStatus.ERROR) {
    this.setGDataStatus_(VolumeManager.GDataStatus.UNMOUNTED);
  }
  var self = this;
  var timeout = setTimeout(function() {
    if (self.getGDataStatus() == VolumeManager.GDataStatus.UNMOUNTED)
      self.setGDataStatus_(VolumeManager.GDataStatus.MOUNTING);
    timeout = null;
  }, VolumeManager.MOUNTING_DELAY);
  this.mount_('', 'gdata', function(mountPath) {
    successCallback(mountPath);
  }, function(error) {
    if (self.getGDataStatus() != VolumeManager.GDataStatus.MOUNTED)
      self.setGDataStatus_(VolumeManager.GDataStatus.ERROR);
    if (timeout != null)
      clearTimeout(timeout);
    errorCallback(error);
  });
};

/**
 * @param {string} fullPath Path to the archive file.
 * @param {Function} successCallback Success callback.
 * @param {Function} errorCallback Error callback.
 */
VolumeManager.prototype.mountArchive = function(fullPath, successCallback,
                                                errorCallback) {
  this.mount_(util.makeFilesystemUrl(fullPath),
      'file', successCallback, errorCallback);
};

/**
 * Unmounts volume.
 * @param {string} mountPath Volume mounted path.
 * @param {Function} successCallback Success callback.
 * @param {Function} errorCallback Error callback.
 */
VolumeManager.prototype.unmount = function(mountPath,
                                           successCallback,
                                           errorCallback) {
  this.validateMountPath_(mountPath);
  var volumeInfo = this.mountedVolumes_[mountPath];
  if (!volumeInfo) {
    errorCallback(VolumeManager.Error.NOT_MOUNTED);
    return;
  }

  chrome.fileBrowserPrivate.removeMount(util.makeFilesystemUrl(mountPath));
  var requestKey = this.makeRequestKey_('unmount', '', volumeInfo.mountPath);
  this.startRequest_(requestKey, successCallback, errorCallback);
};

/**
 * @param {string} mountPath Volume mounted path.
 * @return {VolumeManager.Error?} Returns mount error code
 *                                or undefined if no error.
 */
VolumeManager.prototype.getMountError = function(mountPath) {
  return this.getVolumeInfo_(mountPath).error;
};

/**
 * @param {string} mountPath Volume mounted path.
 * @return {boolean} True if volume at |mountedPath| is mounted but not usable.
 */
VolumeManager.prototype.isUnreadable = function(mountPath) {
  var error = this.getMountError(mountPath);
  return error == VolumeManager.Error.UNKNOWN_FILESYSTEM ||
         error == VolumeManager.Error.UNSUPPORTED_FILESYSTEM;
};

/**
 * @param {string} mountPath Volume mounted path.
 * @return {string} Device type ('usb'|'sd'|'optical'|'mobile'|'unknown')
 *   (as defined in chrome/browser/chromeos/disks/disk_mount_manager.cc).
 */
VolumeManager.prototype.getDeviceType = function(mountPath) {
  return this.getVolumeInfo_(mountPath).deviceType;
};

/**
 * @param {string} mountPath Volume mounted path.
 * @return {boolean} True if volume at |mountedPath| is read only.
 */
VolumeManager.prototype.isReadOnly = function(mountPath) {
  return !!this.getVolumeInfo_(mountPath).readonly;
};

/**
 * Helper method.
 * @param {string} mountPath Volume mounted path.
 * @return {Object} Structure created in |startRequest_|.
 * @private
 */
VolumeManager.prototype.getVolumeInfo_ = function(mountPath) {
  this.validateMountPath_(mountPath);
  return this.mountedVolumes_[mountPath] || {};
};

/**
 * @param {string} url URL for for |fileBrowserPrivate.addMount|.
 * @param {'gdata'|'file'} mountType Mount type for
 *     |fileBrowserPrivate.addMount|.
 * @param {Function} successCallback Success callback.
 * @param {Function} errorCallback Error callback.
 * @private
 */
VolumeManager.prototype.mount_ = function(url, mountType,
                                          successCallback, errorCallback) {
  chrome.fileBrowserPrivate.addMount(url, mountType, {},
                                     function(sourcePath) {
    console.log('Mount request: url=' + url + '; mountType=' + mountType +
                '; sourceUrl=' + sourcePath);
    var requestKey = this.makeRequestKey_('mount', mountType, sourcePath);
    this.startRequest_(requestKey, successCallback, errorCallback);
  }.bind(this));
};

/**
 * @param {sting} key Key produced by |makeRequestKey_|.
 * @param {Function} successCallback To be called when request finishes
 *                                   successfully.
 * @param {Function} errorCallback To be called when request fails.
 * @private
 */
VolumeManager.prototype.startRequest_ = function(key,
    successCallback, errorCallback) {
  if (key in this.requests_) {
    var request = this.requests_[key];
    request.successCallbacks.push(successCallback);
    request.errorCallbacks.push(errorCallback);
  } else {
    this.requests_[key] = {
      successCallbacks: [successCallback],
      errorCallbacks: [errorCallback],

      timeout: setTimeout(this.onTimeout_.bind(this, key),
                          VolumeManager.TIMEOUT)
    };
  }
};

/**
 * Called if no response received in |TIMEOUT|.
 * @param {sting} key Key produced by |makeRequestKey_|.
 * @private
 */
VolumeManager.prototype.onTimeout_ = function(key) {
  this.invokeRequestCallbacks_(this.requests_[key],
                               VolumeManager.Error.TIMEOUT);
  delete this.requests_[key];
};

/**
 * @param {sting} key Key produced by |makeRequestKey_|.
 * @param {VolumeManager.Error|'success'} status Status received from the API.
 * @param {string} opt_mountPath Mount path.
 * @private
 */
VolumeManager.prototype.finishRequest_ = function(key, status, opt_mountPath) {
  var request = this.requests_[key];
  if (!request)
    return;

  clearTimeout(request.timeout);
  this.invokeRequestCallbacks_(request, status, opt_mountPath);
  delete this.requests_[key];
};

/**
 * @param {object} request Structure created in |startRequest_|.
 * @param {VolumeManager.Error|string} status If status == 'success'
 *     success callbacks are called.
 * @param {string} opt_mountPath Mount path. Required if success.
 * @private
 */
VolumeManager.prototype.invokeRequestCallbacks_ = function(request, status,
                                                           opt_mountPath) {
  function callEach(callbacks, self, args) {
    for (var i = 0; i < callbacks.length; i++) {
      callbacks[i].apply(self, args);
    }
  }
  if (status == 'success') {
    callEach(request.successCallbacks, this, [opt_mountPath]);
  } else {
    this.validateError_(status);
    callEach(request.errorCallbacks, this, [status]);
  }
};

/**
 * @param {VolumeManager.Error} error Status string iusually received from API.
 * @private
 */
VolumeManager.prototype.validateError_ = function(error) {
  for (var i in VolumeManager.Error) {
    if (error == VolumeManager.Error[i])
      return;
  }
  throw new Error('Invalid mount error: ', error);
};

/**
 * @param {string} mountPath Mount path.
 * @private
 */
VolumeManager.prototype.validateMountPath_ = function(mountPath) {
  if (!/^\/(((archive|removable)\/[^\/]+)|drive|Downloads)$/.test(mountPath))
    throw new Error('Invalid mount path: ', mountPath);
};
