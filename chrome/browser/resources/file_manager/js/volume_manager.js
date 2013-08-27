// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Represents each volume, such as "drive", "download directory", each "USB
 * flush storage", or "mounted zip archive" etc.
 *
 * @param {string} mountPath Where the volume is mounted.
 * @param {DirectoryEntry} root The root directory entry of this volume.
 * @param {string} error The error if an error is found.
 * @param {string} deviceType The type of device ('usb'|'sd'|'optical'|'mobile'
 *     |'unknown') (as defined in chromeos/disks/disk_mount_manager.cc).
 *     Can be null.
 * @param {boolean} isReadOnly True if the volume is read only.
 * @constructor
 */
function VolumeInfo(mountPath, root, error, deviceType, isReadOnly) {
  // TODO(hidehiko): This should include FileSystem instance.
  this.mountPath = mountPath;
  this.root = root;
  this.error = error;
  this.deviceType = deviceType;
  this.isReadOnly = isReadOnly;

  // VolumeInfo is immutable.
  Object.freeze(this);
}

/**
 * Utilities for volume manager implementation.
 */
var volumeManagerUtil = {};

/**
 * Throws an Error when the given error is not in VolumeManager.Error.
 * @param {VolumeManager.Error} error Status string usually received from APIs.
 */
volumeManagerUtil.validateError = function(error) {
  for (var key in VolumeManager.Error) {
    if (error == VolumeManager.Error[key])
      return;
  }

  throw new Error('Invalid mount error: ' + error);
};

/**
 * The regex pattern which matches valid mount paths.
 * The valid paths are:
 * - Either of '/drive', '/drive_shared_with_me', '/drive_offline',
 *   '/drive_recent' or '/Download'
 * - For archive, drive, removable can have (exactly one) sub directory in the
 *  root path. E.g. '/arhive/foo', '/removable/usb1' etc.
 *
 * @type {RegExp}
 * @private
 */
volumeManagerUtil.validateMountPathRegExp_ = new RegExp(
    '^/(drive|drive_shared_with_me|drive_offline|drive_recent|Downloads|' +
    '((archive|drive|removable)\/[^/]+))$');

/**
 * Throws an Error if the validation fails.
 * @param {string} mountPath The target path of the validation.
 */
volumeManagerUtil.validateMountPath = function(mountPath) {
  if (!volumeManagerUtil.validateMountPathRegExp_.test(mountPath))
    throw new Error('Invalid mount path: ' + mountPath);
};

/**
 * Builds the VolumeInfo data for mountPath.
 * @param {string} mountPath Path to the volume.
 * @param {VolumeManager.Error} error The error string if available.
 * @param {function(Object)} callback Called on completion.
 *     TODO(hidehiko): Replace the type from Object to its original type.
 */
volumeManagerUtil.createVolumeInfo = function(mountPath, error, callback) {
  // Validation of the input.
  volumeManagerUtil.validateMountPath(mountPath);
  if (error)
    volumeManagerUtil.validateError(error);

  // TODO(hidehiko): Do we really need to create a volume info for error
  // case?
  chrome.fileBrowserPrivate.getVolumeMetadata(
      util.makeFilesystemUrl(mountPath),
      function(metadata) {
        if (chrome.runtime.lastError && !error)
          error = VolumeManager.Error.UNKNOWN;

        var deviceType = null;
        var isReadOnly = false;
        if (metadata) {
          deviceType = metadata.deviceType;
          isReadOnly = metadata.isReadOnly;
        }

        // TODO(hidehiko): After multi-filesystem is supported,
        // FileSystem object holds its root entry. So, we won't need this
        // resolving.
        webkitResolveLocalFileSystemURL(
            util.makeFilesystemUrl(mountPath),
            function(entry) {
              callback(new VolumeInfo(
                  mountPath, entry, error, deviceType, isReadOnly));
            },
            function(error) {
              callback(new VolumeInfo(
                  mountPath, null, error, deviceType, isReadOnly));
            });
      });
};

/**
 * The order of the volume list based on root type.
 * @type {Array.<string>}
 * @const
 * @private
 */
volumeManagerUtil.volumeListOrder_ = [
  RootType.DRIVE, RootType.DOWNLOADS, RootType.ARCHIVE, RootType.REMOVABLE
];

/**
 * Compares mount paths to sort the volume list order.
 * @param {string} mountPath1 The mount path for the first volume.
 * @param {string} mountPath2 The mount path for the second volume.
 * @return {number} 0 if mountPath1 and mountPath2 are same, -1 if VolumeInfo
 *     mounted at mountPath1 should be listed before the one mounted at
 *     mountPath2, otherwise 1.
 */
volumeManagerUtil.compareMountPath = function(mountPath1, mountPath2) {
  var order1 = volumeManagerUtil.volumeListOrder_.indexOf(
      PathUtil.getRootType(mountPath1));
  var order2 = volumeManagerUtil.volumeListOrder_.indexOf(
      PathUtil.getRootType(mountPath2));
  if (order1 != order2)
    return order1 < order2 ? -1 : 1;

  if (mountPath1 != mountPath2)
    return mountPath1 < mountPath2 ? -1 : 1;

  // The path is same.
  return 0;
};

/**
 * The container of the VolumeInfo for each mounted volume.
 * @constructor
 */
function VolumeInfoList() {
  /**
   * Holds VolumeInfo instances.
   * @type {cr.ui.ArrayDataModel}
   * @private
   */
  this.model_ = new cr.ui.ArrayDataModel([]);

  Object.freeze(this);
}

VolumeInfoList.prototype = {
  get length() { return this.model_.length; }
};

/**
 * Adds the event listener to listen the change of volume info.
 * @param {string} type The name of the event.
 * @param {function(cr.Event)} handler The handler for the event.
 */
VolumeInfoList.prototype.addEventListener = function(type, handler) {
  this.model_.addEventListener(type, handler);
};

/**
 * Removes the event listener.
 * @param {string} type The name of the event.
 * @param {function(cr.Event)} handler The handler to be removed.
 */
VolumeInfoList.prototype.removeEventListener = function(type, handler) {
  this.model_.removeEventListener(type, handler);
};

/**
 * Adds the volumeInfo to the appropriate position. If there already exists,
 * just replaces it.
 * @param {VolumeInfo} volumeInfo The information of the new volume.
 */
VolumeInfoList.prototype.add = function(volumeInfo) {
  var index = this.findLowerBoundIndex_(volumeInfo.mountPath);
  if (index < this.length &&
      this.item(index).mountPath == volumeInfo.mountPath) {
    // Replace the VolumeInfo.
    this.model_.splice(index, 1, volumeInfo);
  } else {
    // Insert the VolumeInfo.
    this.model_.splice(index, 0, volumeInfo);
  }
};

/**
 * Removes the VolumeInfo of the volume mounted at mountPath.
 * @param {string} mountPath The path to the location where the volume is
 *     mounted.
 */
VolumeInfoList.prototype.remove = function(mountPath) {
  var index = this.findLowerBoundIndex_(mountPath);
  if (index < this.length && this.item(index).mountPath == mountPath)
    this.model_.splice(index, 1);
};

/**
 * Searches the information of the volume mounted at mountPath.
 * @param {string} mountPath The path to the location where the volume is
 *     mounted.
 * @return {VolumeInfo} The volume's information, or null if not found.
 */
VolumeInfoList.prototype.find = function(mountPath) {
  var index = this.findLowerBoundIndex_(mountPath);
  if (index < this.length && this.item(index).mountPath == mountPath)
    return this.item(index);

  // Not found.
  return null;
};

/**
 * @param {string} mountPath The mount path of searched volume.
 * @return {number} The index of the volumee if found, or the inserting
 *     position of the volume.
 * @private
 */
VolumeInfoList.prototype.findLowerBoundIndex_ = function(mountPath) {
  // Assuming the number of elements in the array data model is very small
  // in most cases, use simple linear search, here.
  for (var i = 0; i < this.length; i++) {
    if (volumeManagerUtil.compareMountPath(
            this.item(i).mountPath, mountPath) >= 0)
      return i;
  }
  return this.length;
};

/**
 * @param {number} index The index of the volume in the list.
 * @return {VolumeInfo} The VolumeInfo instance.
 */
VolumeInfoList.prototype.item = function(index) {
  return this.model_.item(index);
};

/**
 * VolumeManager is responsible for tracking list of mounted volumes.
 *
 * @param {Entry} root The root of file system.
 * @constructor
 * @extends {cr.EventTarget}
 */
function VolumeManager(root) {
  /**
   * Root enty of the whole file system.
   * TODO(hidehiko): Remove this when multi-file system is supported.
   * @type {Entry}
   * @private
   */
  this.root_ = root;

  /**
   * The list of archives requested to mount. We will show contents once
   * archive is mounted, but only for mounts from within this filebrowser tab.
   * @type {Object.<string, Object>}
   * @private
   */
  this.requests_ = {};

  /**
   * The list of VolumeInfo instances for each mounted volume.
   * @type {VolumeInfoList}
   */
  this.volumeInfoList = new VolumeInfoList();

  /**
   * True, if mount points have been initialized.
   * @type {boolean}
   * @private
   */
  this.ready_ = false;

  /**
   * True if Drive file system is enabled.
   * TODO(hidehiko): This should be migrated into the multi-file system
   * support. If drive file system is disabled, Files.app should receive
   * unmounted event and UI should hide Drive.
   * @type {boolean}
   */
  this.driveEnabled = false;

  this.initMountPoints_();
  this.driveStatus_ = VolumeManager.DriveStatus.UNMOUNTED;

  this.driveConnectionState_ = {
      type: VolumeManager.DriveConnectionType.OFFLINE,
      reasons: VolumeManager.DriveConnectionType.NO_SERVICE
  };

  chrome.fileBrowserPrivate.onDriveConnectionStatusChanged.addListener(
      this.onDriveConnectionStatusChanged_.bind(this));
  this.onDriveConnectionStatusChanged_();
}

/**
 * Invoked when the drive connection status is changed.
 * @private_
 */
VolumeManager.prototype.onDriveConnectionStatusChanged_ = function() {
  chrome.fileBrowserPrivate.getDriveConnectionState(function(state) {
    this.driveConnectionState_ = state;
    cr.dispatchSimpleEvent(this, 'drive-connection-changed');
  }.bind(this));
};

/**
 * Returns the drive connection state.
 * @return {VolumeManager.DriveConnectionType} Connection type.
 */
VolumeManager.prototype.getDriveConnectionState = function() {
  return this.driveConnectionState_;
};

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
 * List of connection types of drive.
 *
 * Keep this in sync with the kDriveConnectionType* constants in
 * file_browser_private_api.cc.
 *
 * @enum {string}
 */
VolumeManager.DriveConnectionType = {
  OFFLINE: 'offline',  // Connection is offline or drive is unavailable.
  METERED: 'metered',  // Connection is metered. Should limit traffic.
  ONLINE: 'online'     // Connection is online.
};

/**
 * List of reasons of DriveConnectionType.
 *
 * Keep this in sync with the kDriveConnectionReason constants in
 * file_browser_private_api.cc.
 *
 * @enum {string}
 */
VolumeManager.DriveConnectionReason = {
  NOT_READY: 'not_ready',    // Drive is not ready or authentication is failed.
  NO_NETWORK: 'no_network',  // Network connection is unavailable.
  NO_SERVICE: 'no_service'   // Drive service is unavailable.
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
 * The singleton instance of VolumeManager. Initialized by the first invocation
 * of getInstance().
 * @type {VolumeManager}
 * @private
 */
VolumeManager.instance_ = null;

/**
 * The queue of pending getInstance invocations.
 * @type {AsyncUtil.Queue}
 * @private
 */
VolumeManager.getInstanceQueue_ = new AsyncUtil.Queue();

/**
 * @param {function(VolumeManager)} callback Callback to obtain VolumeManager
 *     instance.
 */
VolumeManager.getInstance = function(callback) {
  VolumeManager.getInstanceQueue_.run(function(completionCallback) {
    if (VolumeManager.instance_) {
      callback(VolumeManager.instance_);
      completionCallback();
      return;
    }

    chrome.fileBrowserPrivate.requestFileSystem(function(filesystem) {
      VolumeManager.instance_ = new VolumeManager(filesystem.root);
      callback(VolumeManager.instance_);
      completionCallback();
    });
  });
};


/**
 * Enables/disables Drive file system. Dispatches
 * 'drive-enabled-status-changed' event, when the enabled state is actually
 * changed.
 * @param {boolean} enabled True if Drive file system is enabled.
 */
VolumeManager.prototype.setDriveEnabled = function(enabled) {
  console.error('VolumeManager: setDriveEnabled: ' + enabled);
  if (this.driveEnabled == enabled)
    return;
  this.driveEnabled = enabled;

  // When drive is enabled, start to mount.
  if (enabled)
    this.mountDrive(function() {}, function() {});

  var e = new cr.Event('drive-enabled-status-changed');
  e.enabled = enabled;
  this.dispatchEvent(e);
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
  volumeManagerUtil.validateMountPath(mountPath);
  return !!this.volumeInfoList.find(mountPath);
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
  this.deferredQueue_ = [];
  chrome.fileBrowserPrivate.getMountPoints(function(mountPointList) {
    // According to the C++ implementation, getMountPoints only looks at
    // the connected devices (such as USB memory), and doesn't return anything
    // about Drive, Downloads nor archives.
    // TODO(hidehiko): Figure out the historical intention of the method,
    // and clean them up.

    // Create VolumeInfo for each mount point.
    var group = new AsyncUtil.Group();
    for (var i = 0; i < mountPointList.length; i++) {
      var mountPoint = mountPointList[i];

      // TODO(hidehiko): It should be ok that the drive is mounted already.
      if (mountPoint.mountType == 'drive')
        console.error('Drive is not expected initially mounted');

      var error = mountPoint.mountCondition ?
          'error_' + mountPoint.mountCondition : '';
      group.add(function(mountPoint, error, callback) {
        volumeManagerUtil.createVolumeInfo(
            '/' + mountPoint.mountPath, error,
            function(volumeInfo) {
              this.volumeInfoList.add(volumeInfo);
              callback();
            }.bind(this));
      }.bind(this, mountPoint, error));
    }

    // Then, finalize the initialization.
    group.run(function() {
      // Subscribe to the mount completed event when mount points initialized.
      chrome.fileBrowserPrivate.onMountCompleted.addListener(
          this.onMountCompleted_.bind(this));

      // Run pending tasks.
      var deferredQueue = this.deferredQueue_;
      this.deferredQueue_ = null;
      for (var i = 0; i < deferredQueue.length; i++) {
        deferredQueue[i]();
      }

      // Now, the initialization is completed. Set the state to the ready.
      this.ready_ = true;

      // Notify event listeners that the initialization is done.
      cr.dispatchSimpleEvent(this, 'ready');
      if (mountPointList.length > 0)
        cr.dispatchSimpleEvent(self, 'change');
    }.bind(this));
  }.bind(this));
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
      volumeManagerUtil.createVolumeInfo(
          event.mountPath, error, function(volume) {
            this.volumeInfoList.add(volume);
            this.finishRequest_(requestKey, event.status, event.mountPath);
            cr.dispatchSimpleEvent(this, 'change');
          }.bind(this));
    } else {
      console.warn('No mount path.');
      this.finishRequest_(requestKey, event.status);
    }
  } else if (event.eventType == 'unmount') {
    var mountPath = event.mountPath;
    volumeManagerUtil.validateMountPath(mountPath);
    var status = event.status;
    if (status == VolumeManager.Error.PATH_UNMOUNTED) {
      console.warn('Volume already unmounted: ', mountPath);
      status = 'success';
    }
    var requestKey = this.makeRequestKey_('unmount', '', event.mountPath);
    var requested = requestKey in this.requests_;
    if (event.status == 'success' && !requested &&
        this.volumeInfoList.find(mountPath)) {
      console.warn('Mounted volume without a request: ', mountPath);
      var e = new cr.Event('externally-unmounted');
      e.mountPath = mountPath;
      this.dispatchEvent(e);
    }
    this.finishRequest_(requestKey, status);

    if (event.status == 'success') {
      this.volumeInfoList.remove(mountPath);
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
  this.root_.getDirectory(
      mountPath, {},
      function(entry) {
        // After file system is mounted, we need to "read" drive grand root
        // entry at first. It loads mydrive root entry as a part of
        // 'fast-fetch' quickly, and starts full feed fetch in parallel.
        // Without this read, accessing mydrive root will be 'full-fetch'
        // rather than 'fast-fetch' on the current architecture.
        // Just "getting" the grand root entry doesn't trigger it. Rather,
        // it starts when the entry is "read".
        entry.createReader().readEntries(
            callback.bind(null, true),
            callback.bind(null, false));
      },
      callback.bind(null, false));
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
 * @param {function(string)} successCallback Success callback.
 * @param {function(VolumeManager.Error)} errorCallback Error callback.
 */
VolumeManager.prototype.mountDrive = function(successCallback, errorCallback) {
  if (this.getDriveStatus() == VolumeManager.DriveStatus.ERROR) {
    this.setDriveStatus_(VolumeManager.DriveStatus.UNMOUNTED);
  }
  this.setDriveStatus_(VolumeManager.DriveStatus.MOUNTING);
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
 * @param {function(string)} successCallback Success callback.
 * @param {function(VolumeManager.Error)} errorCallback Error callback.
 */
VolumeManager.prototype.mountArchive = function(fileUrl, successCallback,
                                                errorCallback) {
  this.mount_(fileUrl, 'file', successCallback, errorCallback);
};

/**
 * Unmounts volume.
 * @param {string} mountPath Volume mounted path.
 * @param {function(string)} successCallback Success callback.
 * @param {function(VolumeManager.Error)} errorCallback Error callback.
 */
VolumeManager.prototype.unmount = function(mountPath,
                                           successCallback,
                                           errorCallback) {
  volumeManagerUtil.validateMountPath(mountPath);
  if (this.deferredQueue_) {
    this.deferredQueue_.push(this.unmount.bind(this,
        mountPath, successCallback, errorCallback));
    return;
  }

  var volumeInfo = this.volumeInfoList.find(mountPath);
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
 * @return {VolumeInfo} The data about the volume.
 */
VolumeManager.prototype.getVolumeInfo = function(mountPath) {
  volumeManagerUtil.validateMountPath(mountPath);
  return this.volumeInfoList.find(mountPath);
};

/**
 * @param {string} url URL for for |fileBrowserPrivate.addMount|.
 * @param {'drive'|'file'} mountType Mount type for
 *     |fileBrowserPrivate.addMount|.
 * @param {function(string)} successCallback Success callback.
 * @param {function(VolumeManager.Error)} errorCallback Error callback.
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
    console.info('Mount request: url=' + url + '; mountType=' + mountType +
                 '; sourceUrl=' + sourcePath);
    var requestKey = this.makeRequestKey_('mount', mountType, sourcePath);
    this.startRequest_(requestKey, successCallback, errorCallback);
  }.bind(this));
};

/**
 * @param {string} key Key produced by |makeRequestKey_|.
 * @param {function(string)} successCallback To be called when request finishes
 *     successfully.
 * @param {function(VolumeManager.Error)} errorCallback To be called when
 *     request fails.
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
    volumeManagerUtil.validateError(status);
    callEach(request.errorCallbacks, this, [status]);
  }
};
