// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

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

  /**
   * True, if mount points have been initialized.
   * @type {boolean}
   * @private
   */
  this.ready_ = false;

  this.initMountPoints_();
  this.driveStatus_ = VolumeManager.DriveStatus.UNMOUNTED;
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
  AUTHENTICATION: 'error_authentication',
  PATH_UNMOUNTED: 'error_path_unmounted'
};

/**
 * @enum
 */
VolumeManager.DriveStatus = {
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
 * Delay in milliseconds DRIVE changes its state from |UNMOUNTED| to
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
 * @param {VolumeManager.DriveStatus} newStatus New DRIVE status.
 * @private
 */
VolumeManager.prototype.setDriveStatus_ = function(newStatus) {
  if (this.driveStatus_ != newStatus) {
    this.driveStatus_ = newStatus;
    cr.dispatchSimpleEvent(this, 'drive-status-changed');
  }
};

/**
 * @return {VolumeManager.DriveStatus} Current DRIVE status.
 */
VolumeManager.prototype.getDriveStatus = function() {
  return this.driveStatus_;
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
 * @return {boolean} True if already initialized.
 */
VolumeManager.prototype.isReady = function() {
  return this.ready_;
};

/**
 * Initialized mount points.
 * @private
 */
VolumeManager.prototype.initMountPoints_ = function() {
  var mountedVolumes = [];
  var self = this;
  var index = 0;
  this.deferredQueue_ = [];
  var step = function(mountPoints) {
    if (index < mountPoints.length) {
      var info = mountPoints[index];
      if (info.mountType == 'drive')
        console.error('Drive is not expected initially mounted');
      var error = info.mountCondition ? 'error_' + info.mountCondition : '';
      var onVolumeInfo = function(volume) {
        mountedVolumes.push(volume);
        index++;
        step(mountPoints);
      };
      self.makeVolumeInfo_('/' + info.mountPath, error, onVolumeInfo);
    } else {
      for (var i = 0; i < mountedVolumes.length; i++) {
        var volume = mountedVolumes[i];
        self.mountedVolumes_[volume.mountPath] = volume;
      }

      // Subscribe to the mount completed event when mount points initialized.
      chrome.fileBrowserPrivate.onMountCompleted.addListener(
          self.onMountCompleted_.bind(self));

      var deferredQueue = self.deferredQueue_;
      self.deferredQueue_ = null;
      for (var i = 0; i < deferredQueue.length; i++) {
        deferredQueue[i]();
      }

      cr.dispatchSimpleEvent(self, 'ready');
      self.ready_ = true;
      if (mountedVolumes.length > 0)
        cr.dispatchSimpleEvent(self, 'change');
    }
  };

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

  if (event.mountType == 'drive') {
    if (event.status == 'success') {
      if (event.eventType == 'mount') {
        // If the mount is not requested, the mount status will not be changed
        // at mountDrive(). Sets it here in such a case.
        var self = this;
        var timeout = setTimeout(function() {
          if (self.getDriveStatus() == VolumeManager.DriveStatus.UNMOUNTED)
            self.setDriveStatus_(VolumeManager.DriveStatus.MOUNTING);
          timeout = null;
        }, VolumeManager.MOUNTING_DELAY);

        this.waitDriveLoaded_(event.mountPath, function(success) {
          if (timeout != null)
            clearTimeout(timeout);
          this.setDriveStatus_(success ? VolumeManager.DriveStatus.MOUNTED :
                                         VolumeManager.DriveStatus.ERROR);
        }.bind(this));
      } else if (event.eventType == 'unmount') {
        this.setDriveStatus_(VolumeManager.DriveStatus.UNMOUNTED);
      }
    } else {
      this.setDriveStatus_(VolumeManager.DriveStatus.ERROR);
    }
  }
};

/**
 * First access to Drive takes time (to fetch data from the cloud).
 * We want to change state to MOUNTED (likely from MOUNTING) when the
 * drive ready to operate.
 *
 * @param {string} mountPath Drive mount path.
 * @param {function(boolean, FileError=)} callback To be called when waiting
 *     finishes. If the case of error, there may be a FileError parameter.
 * @private
 */
VolumeManager.prototype.waitDriveLoaded_ = function(mountPath, callback) {
  chrome.fileBrowserPrivate.requestLocalFileSystem(function(filesystem) {
    filesystem.root.getDirectory(mountPath, {},
        function(entry) {
            // After introducion of the 'fast-fetch' feature, getting the root
            // entry does not start fetching data. Rather, it starts when the
            // entry is read.
            entry.createReader().readEntries(
                callback.bind(null, true),
                callback.bind(null, false));
        },
        callback.bind(null, false));
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
  var onVolumeMetadata = function(metadata) {
   callback({
     mountPath: mountPath,
     error: error,
     deviceType: metadata && metadata.deviceType,
     readonly: !!metadata && metadata.isReadOnly
   });
  };
  chrome.fileBrowserPrivate.getVolumeMetadata(
      util.makeFilesystemUrl(mountPath), onVolumeMetadata);
};

/**
 * Creates string to match mount events with requests.
 * @param {string} requestType 'mount' | 'unmount'.
 * @param {string} mountType 'device' | 'file' | 'network' | 'drive'.
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
 * @param {function} successCallback Success callback.
 * @param {function} errorCallback Error callback.
 */
VolumeManager.prototype.mountDrive = function(successCallback, errorCallback) {
  if (this.getDriveStatus() == VolumeManager.DriveStatus.ERROR) {
    this.setDriveStatus_(VolumeManager.DriveStatus.UNMOUNTED);
  }
  var self = this;
  this.mount_('', 'drive', function(mountPath) {
    this.waitDriveLoaded_(mountPath, function(success, error) {
      if (success) {
        successCallback(mountPath);
      } else {
        errorCallback(error);
      }
    });
  }, function(error) {
    if (self.getDriveStatus() != VolumeManager.DriveStatus.MOUNTED)
      self.setDriveStatus_(VolumeManager.DriveStatus.ERROR);
    errorCallback(error);
  });
};

/**
 * @param {string} fileUrl File url to the archive file.
 * @param {function} successCallback Success callback.
 * @param {function} errorCallback Error callback.
 */
VolumeManager.prototype.mountArchive = function(fileUrl, successCallback,
                                                errorCallback) {
  this.mount_(fileUrl, 'file', successCallback, errorCallback);
};

/**
 * Unmounts volume.
 * @param {string} mountPath Volume mounted path.
 * @param {function} successCallback Success callback.
 * @param {function} errorCallback Error callback.
 */
VolumeManager.prototype.unmount = function(mountPath,
                                           successCallback,
                                           errorCallback) {
  this.validateMountPath_(mountPath);
  if (this.deferredQueue_) {
    this.deferredQueue_.push(this.unmount.bind(this,
        mountPath, successCallback, errorCallback));
    return;
  }

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
 *   (as defined in chromeos/disks/disk_mount_manager.cc).
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
 * @param {'drive'|'file'} mountType Mount type for
 *     |fileBrowserPrivate.addMount|.
 * @param {function} successCallback Success callback.
 * @param {function} errorCallback Error callback.
 * @private
 */
VolumeManager.prototype.mount_ = function(url, mountType,
                                          successCallback, errorCallback) {
  if (this.deferredQueue_) {
    this.deferredQueue_.push(this.mount_.bind(this,
        url, mountType, successCallback, errorCallback));
    return;
  }

  chrome.fileBrowserPrivate.addMount(url, mountType, {},
                                     function(sourcePath) {
    console.log('Mount request: url=' + url + '; mountType=' + mountType +
                '; sourceUrl=' + sourcePath);
    var requestKey = this.makeRequestKey_('mount', mountType, sourcePath);
    this.startRequest_(requestKey, successCallback, errorCallback);
  }.bind(this));
};

/**
 * @param {string} key Key produced by |makeRequestKey_|.
 * @param {function} successCallback To be called when request finishes
 *                                   successfully.
 * @param {function} errorCallback To be called when request fails.
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
 * @param {string} key Key produced by |makeRequestKey_|.
 * @private
 */
VolumeManager.prototype.onTimeout_ = function(key) {
  this.invokeRequestCallbacks_(this.requests_[key],
                               VolumeManager.Error.TIMEOUT);
  delete this.requests_[key];
};

/**
 * @param {string} key Key produced by |makeRequestKey_|.
 * @param {VolumeManager.Error|'success'} status Status received from the API.
 * @param {string=} opt_mountPath Mount path.
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
 * @param {Object} request Structure created in |startRequest_|.
 * @param {VolumeManager.Error|string} status If status == 'success'
 *     success callbacks are called.
 * @param {string=} opt_mountPath Mount path. Required if success.
 * @private
 */
VolumeManager.prototype.invokeRequestCallbacks_ = function(request, status,
                                                           opt_mountPath) {
  var callEach = function(callbacks, self, args) {
    for (var i = 0; i < callbacks.length; i++) {
      callbacks[i].apply(self, args);
    }
  };
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
  console.log(mountPath);
  if (!/^\/(drive|drive_offline|Downloads)$/.test(mountPath) &&
      !/^\/((archive|removable|drive)\/[^\/]+)$/.test(mountPath))
    throw new Error('Invalid mount path: ', mountPath);
};
