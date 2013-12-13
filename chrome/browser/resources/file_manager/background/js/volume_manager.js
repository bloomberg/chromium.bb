// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Represents each volume, such as "drive", "download directory", each "USB
 * flush storage", or "mounted zip archive" etc.
 *
 * @param {util.VolumeType} volumeType The type of the volume.
 * @param {string} mountPath Where the volume is mounted.
 * @param {string} volumeId ID of the volume.
 * @param {DirectoryEntry} root The root directory entry of this volume.
 * @param {string} error The error if an error is found.
 * @param {string} deviceType The type of device ('usb'|'sd'|'optical'|'mobile'
 *     |'unknown') (as defined in chromeos/disks/disk_mount_manager.cc).
 *     Can be null.
 * @param {boolean} isReadOnly True if the volume is read only.
 * @param {!{displayName:string, isCurrentProfile:boolean}} profile Profile
 *     information.
 * @constructor
 */
function VolumeInfo(
    volumeType,
    mountPath,
    volumeId,
    root,
    error,
    deviceType,
    isReadOnly,
    profile) {
  this.volumeType = volumeType;
  // TODO(hidehiko): This should include FileSystem instance.
  this.mountPath = mountPath;
  this.volumeId = volumeId;
  this.root = root;

  // Note: This represents if the mounting of the volume is successfully done
  // or not. (If error is empty string, the mount is successfully done).
  // TODO(hidehiko): Rename to make this more understandable.
  this.error = error;
  this.deviceType = deviceType;
  this.isReadOnly = isReadOnly;
  this.profile = Object.freeze(profile);

  // VolumeInfo is immutable.
  Object.freeze(this);
}

/**
 * Obtains a URL of the display root directory that users can see as a root.
 * @return {string} URL of root entry.
 */
VolumeInfo.prototype.getDisplayRootDirectoryURL = function() {
  return this.root.toURL() +
      (this.volumeType === util.VolumeType.DRIVE ? '/root' : '');
};

/**
 * Obtains volume label.
 * @return {string} Label for the volume.
 */
VolumeInfo.prototype.getLabel = function() {
  if (this.volumeType === util.VolumeType.DRIVE)
    return str('DRIVE_DIRECTORY_LABEL');
  else
    return PathUtil.getFolderLabel(this.mountPath);
};

/**
 * Utilities for volume manager implementation.
 */
var volumeManagerUtil = {};

/**
 * Throws an Error when the given error is not in util.VolumeError.
 * @param {util.VolumeError} error Status string usually received from APIs.
 */
volumeManagerUtil.validateError = function(error) {
  for (var key in util.VolumeError) {
    if (error === util.VolumeError[key])
      return;
  }

  throw new Error('Invalid mount error: ' + error);
};

/**
 * Returns the root entry of a volume mounted at mountPath.
 *
 * @param {string} mountPath The mounted path of the volume.
 * @param {function(DirectoryEntry)} successCallback Called when the root entry
 *     is found.
 * @param {function(FileError)} errorCallback Called when an error is found.
 * @private
 */
volumeManagerUtil.getRootEntry_ = function(
    mountPath, successCallback, errorCallback) {
  // We always request FileSystem here, because requestFileSystem() grants
  // permissions if necessary, especially for Drive File System at first mount
  // time.
  // Note that we actually need to request FileSystem after multi file system
  // support, so this will be more natural code then.
  chrome.fileBrowserPrivate.requestFileSystem(
      'compatible',
      function(fileSystem) {
        // TODO(hidehiko): chrome.runtime.lastError should have error reason.
        if (!fileSystem) {
          errorCallback(util.createFileError(FileError.NOT_FOUND_ERR));
          return;
        }

        fileSystem.root.getDirectory(
            mountPath.substring(1),  // Strip leading '/'.
            {create: false}, successCallback, errorCallback);
      });
};

/**
 * Builds the VolumeInfo data from VolumeMetadata.
 * @param {VolumeMetadata} volumeMetadata Metadata instance for the volume.
 * @param {function(VolumeInfo)} callback Called on completion.
 */
volumeManagerUtil.createVolumeInfo = function(volumeMetadata, callback) {
  volumeManagerUtil.getRootEntry_(
      volumeMetadata.mountPath,
      function(entry) {
        if (volumeMetadata.volumeType === util.VolumeType.DRIVE) {
          // After file system is mounted, we "read" drive grand root
          // entry at first. This triggers full feed fetch on background.
          // Note: we don't need to handle errors here, because even if
          // it fails, accessing to some path later will just become
          // a fast-fetch and it re-triggers full-feed fetch.
          entry.createReader().readEntries(
              function() { /* do nothing */ },
              function(error) {
                console.error(
                    'Triggering full feed fetch is failed: ' +
                        util.getFileErrorMnemonic(error.code));
              });
        }
        callback(new VolumeInfo(
            volumeMetadata.volumeType,
            volumeMetadata.mountPath,
            volumeMetadata.volumeId,
            entry,
            volumeMetadata.mountCondition,
            volumeMetadata.deviceType,
            volumeMetadata.isReadOnly,
            volumeMetadata.profile));
      },
      function(fileError) {
        console.error('Root entry is not found: ' +
            volumeMetadata.mountPath + ', ' +
            util.getFileErrorMnemonic(fileError.code));
        callback(new VolumeInfo(
            volumeMetadata.volumeType,
            volumeMetadata.mountPath,
            volumeMetadata.volumeId,
            null,  // Root entry is not found.
            volumeMetadata.mountCondition,
            volumeMetadata.deviceType,
            volumeMetadata.isReadOnly,
            volumeMetadata.profile));
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
  if (order1 !== order2)
    return order1 < order2 ? -1 : 1;

  if (mountPath1 !== mountPath2)
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
 * @param {function(Event)} handler The handler for the event.
 */
VolumeInfoList.prototype.addEventListener = function(type, handler) {
  this.model_.addEventListener(type, handler);
};

/**
 * Removes the event listener.
 * @param {string} type The name of the event.
 * @param {function(Event)} handler The handler to be removed.
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
      this.item(index).mountPath === volumeInfo.mountPath) {
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
  if (index < this.length && this.item(index).mountPath === mountPath)
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
  if (index < this.length && this.item(index).mountPath === mountPath)
    return this.item(index);

  // Not found.
  return null;
};

/**
 * Searches the information of the volume that contains an item pointed by the
 * path.
 * @param {string} path Path pointing an entry on a volume.
 * @return {VolumeInfo} The volume's information, or null if not found.
 */
VolumeInfoList.prototype.findByPath = function(path) {
  for (var i = 0; i < this.length; i++) {
    var mountPath = this.item(i).mountPath;
    if (path === mountPath || path.indexOf(mountPath + '/') === 0)
      return this.item(i);
  }
  return null;
};

/**
 * @param {string} mountPath The mount path of searched volume.
 * @return {number} The index of the volume if found, or the inserting
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
   * The list of VolumeInfo instances for each mounted volume.
   * @type {VolumeInfoList}
   */
  this.volumeInfoList = new VolumeInfoList();

  // The status should be merged into VolumeManager.
  // TODO(hidehiko): Remove them after the migration.
  this.driveConnectionState_ = {
    type: util.DriveConnectionType.OFFLINE,
    reason: util.DriveConnectionReason.NO_SERVICE
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
 * @return {util.DriveConnectionType} Connection type.
 */
VolumeManager.prototype.getDriveConnectionState = function() {
  return this.driveConnectionState_;
};

/**
 * VolumeManager extends cr.EventTarget.
 */
VolumeManager.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Time in milliseconds that we wait a response for. If no response on
 * mount/unmount received the request supposed failed.
 */
VolumeManager.TIMEOUT = 15 * 60 * 1000;

/**
 * Queue to run getInstance sequentially.
 * @type {AsyncUtil.Queue}
 * @private
 */
VolumeManager.getInstanceQueue_ = new AsyncUtil.Queue();

/**
 * The singleton instance of VolumeManager. Initialized by the first invocation
 * of getInstance().
 * @type {VolumeManager}
 * @private
 */
VolumeManager.instance_ = null;

/**
 * Returns the VolumeManager instance asynchronously. If it is not created or
 * under initialization, it will waits for the finish of the initialization.
 * @param {function(VolumeManager)} callback Called with the VolumeManager
 *     instance.
 */
VolumeManager.getInstance = function(callback) {
  VolumeManager.getInstanceQueue_.run(function(continueCallback) {
    if (VolumeManager.instance_) {
      callback(VolumeManager.instance_);
      continueCallback();
      return;
    }

    VolumeManager.instance_ = new VolumeManager();
    VolumeManager.instance_.initialize_(function() {
      callback(VolumeManager.instance_);
      continueCallback();
    });
  });
};

/**
 * Initializes mount points.
 * @param {function()} callback Called upon the completion of the
 *     initialization.
 * @private
 */
VolumeManager.prototype.initialize_ = function(callback) {
  chrome.fileBrowserPrivate.getVolumeMetadataList(function(volumeMetadataList) {
    // Create VolumeInfo for each volume.
    var group = new AsyncUtil.Group();
    for (var i = 0; i < volumeMetadataList.length; i++) {
      group.add(function(volumeMetadata, continueCallback) {
        volumeManagerUtil.createVolumeInfo(
            volumeMetadata,
            function(volumeInfo) {
              this.volumeInfoList.add(volumeInfo);
              if (volumeMetadata.volumeType === util.VolumeType.DRIVE)
                this.onDriveConnectionStatusChanged_();
              continueCallback();
            }.bind(this));
      }.bind(this, volumeMetadataList[i]));
    }

    // Then, finalize the initialization.
    group.run(function() {
      // Subscribe to the mount completed event when mount points initialized.
      chrome.fileBrowserPrivate.onMountCompleted.addListener(
          this.onMountCompleted_.bind(this));
      callback();
    }.bind(this));
  }.bind(this));
};

/**
 * Event handler called when some volume was mounted or unmounted.
 * @param {MountCompletedEvent} event Received event.
 * @private
 */
VolumeManager.prototype.onMountCompleted_ = function(event) {
  if (event.eventType === 'mount') {
    if (event.volumeMetadata.mountPath) {
      var requestKey = this.makeRequestKey_(
          'mount',
          event.volumeMetadata.sourcePath);

      var error = event.status === 'success' ? '' : event.status;

      volumeManagerUtil.createVolumeInfo(
          event.volumeMetadata,
          function(volumeInfo) {
            this.volumeInfoList.add(volumeInfo);
            this.finishRequest_(requestKey, event.status, volumeInfo.mountPath);

            if (volumeInfo.volumeType === util.VolumeType.DRIVE) {
              // Update the network connection status, because until the
              // drive is initialized, the status is set to not ready.
              // TODO(hidehiko): The connection status should be migrated into
              // VolumeMetadata.
              this.onDriveConnectionStatusChanged_();
            }
          }.bind(this));
    } else {
      console.warn('No mount path.');
      this.finishRequest_(requestKey, event.status);
    }
  } else if (event.eventType === 'unmount') {
    var mountPath = event.volumeMetadata.mountPath;
    var status = event.status;
    if (status === util.VolumeError.PATH_UNMOUNTED) {
      console.warn('Volume already unmounted: ', mountPath);
      status = 'success';
    }
    var requestKey = this.makeRequestKey_('unmount', mountPath);
    var requested = requestKey in this.requests_;
    var volumeInfo = this.volumeInfoList.find(mountPath);
    if (event.status === 'success' && !requested && volumeInfo) {
      console.warn('Mounted volume without a request: ', mountPath);
      var e = new Event('externally-unmounted');
      // TODO(mtomasz): The mountPath field is deprecated. Remove it.
      e.mountPath = mountPath;
      e.volumeInfo = volumeInfo;
      this.dispatchEvent(e);
    }
    this.finishRequest_(requestKey, status);

    if (event.status === 'success')
      this.volumeInfoList.remove(mountPath);
  }
};

/**
 * Creates string to match mount events with requests.
 * @param {string} requestType 'mount' | 'unmount'. TODO(hidehiko): Replace by
 *     enum.
 * @param {string} path Source path provided by API for mount request, or
 *     mount path for unmount request.
 * @return {string} Key for |this.requests_|.
 * @private
 */
VolumeManager.prototype.makeRequestKey_ = function(requestType, path) {
  return requestType + ':' + path;
};

/**
 * @param {string} fileUrl File url to the archive file.
 * @param {function(string)} successCallback Success callback.
 * @param {function(util.VolumeError)} errorCallback Error callback.
 */
VolumeManager.prototype.mountArchive = function(
    fileUrl, successCallback, errorCallback) {
  chrome.fileBrowserPrivate.addMount(fileUrl, function(sourcePath) {
    console.info(
        'Mount request: url=' + fileUrl + '; sourceUrl=' + sourcePath);
    var requestKey = this.makeRequestKey_('mount', sourcePath);
    this.startRequest_(requestKey, successCallback, errorCallback);
  }.bind(this));
};

/**
 * Unmounts volume.
 * @param {string} mountPath Volume mounted path.
 * @param {function(string)} successCallback Success callback.
 * @param {function(util.VolumeError)} errorCallback Error callback.
 */
VolumeManager.prototype.unmount = function(mountPath,
                                           successCallback,
                                           errorCallback) {
  var volumeInfo = this.volumeInfoList.find(mountPath);
  if (!volumeInfo) {
    errorCallback(util.VolumeError.NOT_MOUNTED);
    return;
  }

  chrome.fileBrowserPrivate.removeMount(util.makeFilesystemUrl(mountPath));
  var requestKey = this.makeRequestKey_('unmount', volumeInfo.mountPath);
  this.startRequest_(requestKey, successCallback, errorCallback);
};

/**
 * Resolves the absolute path to its entry. Shouldn't be used outside of the
 * Files app's initialization.
 * @param {string} path The path to be resolved.
 * @param {function(Entry)} successCallback Called with the resolved entry on
 *     success.
 * @param {function(FileError)} errorCallback Called on error.
 */
VolumeManager.prototype.resolveAbsolutePath = function(
    path, successCallback, errorCallback) {
  // Make sure the path is in the mounted volume.
  var volumeInfo = this.getVolumeInfo(path);
  if (!volumeInfo || !volumeInfo.root) {
    errorCallback(util.createFileError(FileError.NOT_FOUND_ERR));
    return;
  }

  webkitResolveLocalFileSystemURL(
      util.makeFilesystemUrl(path), successCallback, errorCallback);
};

/**
 * Obtains the information of the volume that containing an entry pointed by the
 * specified path.
 * TODO(hirono): Stop to use path to get a volume info.
 *
 * @param {string|Entry} target Path or Entry pointing anywhere on a volume.
 * @return {VolumeInfo} The data about the volume.
 */
VolumeManager.prototype.getVolumeInfo = function(target) {
  if (typeof target === 'string')
    return this.volumeInfoList.findByPath(target);
  else if (util.isFakeEntry(target))
    return this.getCurrentProfileVolumeInfo(util.VolumeType.DRIVE);
  else
    return this.volumeInfoList.findByPath(target.fullPath);
};

/**
 * Obtains a volume information from a file entry URL.
 * TODO(hirono): Check a file system to find a volume.
 *
 * @param {string} url URL of entry.
 * @return {VolumeInfo} Volume info.
 */
VolumeManager.prototype.getVolumeInfoByURL = function(url) {
  return this.getVolumeInfo(util.extractFilePath(url));
};

/**
 * Obtains a volume infomration of the current profile.
 *
 * @param {util.VolumeType} volumeType Volume type.
 * @return {VolumeInfo} Volume info.
 */
VolumeManager.prototype.getCurrentProfileVolumeInfo = function(volumeType) {
  for (var i = 0; i < this.volumeInfoList.length; i++) {
    var volumeInfo = this.volumeInfoList.item(i);
    if (volumeInfo.profile.isCurrentProfile &&
        volumeInfo.volumeType === volumeType)
      return volumeInfo;
  }
  return null;
};

/**
 * Obtains location information from an entry.
 *
 * @param {Entry|Object} entry File or directory entry. It can be a fake entry.
 * @return {EntryLocation} Location information.
 */
VolumeManager.prototype.getLocationInfo = function(entry) {
  if (util.isFakeEntry(entry)) {
    return new EntryLocation(
        // TODO(hirono): Specify currect volume.
        this.getCurrentProfileVolumeInfo(RootType.DRIVE),
        entry.rootType,
        true /* the entry points a root directory. */);
  } else {
    return this.getLocationInfoByPath(entry.fullPath);
  }
};

/**
 * Obtains location information from a path.
 * TODO(hirono): Remove the method before introducing separate file system.
 *
 * @param {string} path Path.
 * @return {EntryLocation} Location information.
 */
VolumeManager.prototype.getLocationInfoByPath = function(path) {
  var volumeInfo = this.volumeInfoList.findByPath(path);
  return volumeInfo && PathUtil.getLocationInfo(volumeInfo, path);
};

/**
 * @param {string} key Key produced by |makeRequestKey_|.
 * @param {function(string)} successCallback To be called when request finishes
 *     successfully.
 * @param {function(util.VolumeError)} errorCallback To be called when
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
                               util.VolumeError.TIMEOUT);
  delete this.requests_[key];
};

/**
 * @param {string} key Key produced by |makeRequestKey_|.
 * @param {util.VolumeError|'success'} status Status received from the API.
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
 * @param {util.VolumeError|string} status If status === 'success'
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
  if (status === 'success') {
    callEach(request.successCallbacks, this, [opt_mountPath]);
  } else {
    volumeManagerUtil.validateError(status);
    callEach(request.errorCallbacks, this, [status]);
  }
};
