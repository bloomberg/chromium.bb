// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Represents each volume, such as "drive", "download directory", each "USB
 * flush storage", or "mounted zip archive" etc.
 *
 * @param {util.VolumeType} volumeType The type of the volume.
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
    volumeId,
    root,
    error,
    deviceType,
    isReadOnly,
    profile) {
  this.volumeType_ = volumeType;
  // TODO(hidehiko): This should include FileSystem instance.
  this.volumeId_ = volumeId;
  this.root_ = root;
  this.displayRoot_ = null;
  this.fakeEntries_ = {};
  this.displayRoot_ = null;
  this.displayRootPromise_ = null;

  if (volumeType === util.VolumeType.DRIVE) {
    // TODO(mtomasz): Convert fake entries to DirectoryProvider.
    this.fakeEntries_[RootType.DRIVE_OFFLINE] = {
      isDirectory: true,
      rootType: RootType.DRIVE_OFFLINE,
      toURL: function() { return 'fake-entry://drive_offline' }
    };
    this.fakeEntries_[RootType.DRIVE_SHARED_WITH_ME] = {
      isDirectory: true,
      rootType: RootType.DRIVE_SHARED_WITH_ME,
      toURL: function() { return 'fake-entry://drive_shared_with_me'; }
    };
    this.fakeEntries_[RootType.DRIVE_RECENT] = {
      isDirectory: true,
      rootType: RootType.DRIVE_RECENT,
      toURL: function() { return 'fake-entry://drive_recent'; }
    };
  }

  // Note: This represents if the mounting of the volume is successfully done
  // or not. (If error is empty string, the mount is successfully done).
  // TODO(hidehiko): Rename to make this more understandable.
  this.error = error;
  this.deviceType = deviceType;
  this.isReadOnly = isReadOnly;
  this.profile = Object.freeze(profile);

  Object.seal(this);
}

VolumeInfo.prototype = {
  /**
   * @return {util.VolumeType} Volume type.
   */
  get volumeType() {
    return this.volumeType_;
  },
  /**
   * @return {string} Volume id.
   */
  get volumeId() {
    return this.volumeId_;
  },
  /**
   * @return {DirectoryEntry} Root path.
   */
  get root() {
    return this.root_;
  },
  /**
   * @return {DirectoryEntry} Display root path. It is null before finishing to
   * resolve the entry.
   */
  get displayRoot() {
    return this.displayRoot_;
  },
  /**
   * @return {Object.<string, Object>} Fake entries.
   */
  get fakeEntries() {
    return this.fakeEntries_;
  }
};

/**
 * Starts resolving the display root and obtains it.  It may take long time for
 * Drive. Once resolved, it is cached.
 *
 * @param {function(DirectoryEntry)} onSuccess Success callback with the
 *     display root directory as an argument.
 * @param {function(FileError)} onFailure Failure callback.
 */
VolumeInfo.prototype.resolveDisplayRoot = function(onSuccess, onFailure) {
  if (!this.displayRootPromise_) {
    // TODO(mtomasz): Do not add VolumeInfo which failed to resolve root, and
    // remove this if logic. Call onSuccess() always, instead.
    if (this.volumeType !== util.VolumeType.DRIVE) {
      if (this.root)
        this.displayRootPromise_ = Promise.resolve(this.root);
      else
        this.displayRootPromise_ = Promise.reject(this.error);
    } else {
      // For Drive, we need to resolve.
      var displayRootURL = this.root.toURL() + '/root';
      this.displayRootPromise_ = new Promise(
          webkitResolveLocalFileSystemURL.bind(null, displayRootURL));
    }

    // Store the obtained displayRoot.
    this.displayRootPromise_.then(function(displayRoot) {
      this.displayRoot_ = displayRoot;
    }.bind(this));
  }
  this.displayRootPromise_.then(onSuccess, onFailure);
};

/**
 * Obtains volume label.
 * @return {string} Label for the volume.
 */
VolumeInfo.prototype.getLabel = function() {
  if (this.volumeType === util.VolumeType.DOWNLOADS)
    return str('DOWNLOADS_DIRECTORY_LABEL');
  if (this.volumeType === util.VolumeType.DRIVE)
    return str('DRIVE_DIRECTORY_LABEL');

  // TODO(mtomasz): We are assuming that root is available. Root may not be
  // available only for Drive, and we should fix it. crbug.com/335460
  return this.root.name;
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
 * TODO(mtomasz): Remove this method.
 *
 * @param {string} volumeId ID of the volume to be mounted.
 * @param {function(DirectoryEntry)} successCallback Called when the root entry
 *     is found.
 * @param {function(FileError)} errorCallback Called when an error is found.
 * @private
 */
volumeManagerUtil.getRootEntry_ = function(
    volumeId, successCallback, errorCallback) {
  // We always request FileSystem here, because requestFileSystem() grants
  // permissions if necessary, especially for Drive File System at first mount
  // time.
  // Note that we actually need to request FileSystem after multi file system
  // support, so this will be more natural code then.
  chrome.fileBrowserPrivate.requestFileSystem(
      volumeId,
      function(fileSystem) {
        // TODO(hidehiko): chrome.runtime.lastError should have error reason.
        if (!fileSystem) {
          errorCallback(util.createDOMError(util.FileError.NOT_FOUND_ERR));
          return;
        }

        successCallback(fileSystem.root);
      });
};

/**
 * Builds the VolumeInfo data from VolumeMetadata.
 * @param {VolumeMetadata} volumeMetadata Metadata instance for the volume.
 * @param {function(VolumeInfo)} callback Called on completion.
 */
volumeManagerUtil.createVolumeInfo = function(volumeMetadata, callback) {
  volumeManagerUtil.getRootEntry_(
      volumeMetadata.volumeId,
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
                    'Triggering full feed fetch is failed: ' + error.name);
              });
        }
        callback(new VolumeInfo(
            volumeMetadata.volumeType,
            volumeMetadata.volumeId,
            entry,
            volumeMetadata.mountCondition,
            volumeMetadata.deviceType,
            volumeMetadata.isReadOnly,
            volumeMetadata.profile));
      },
      function(fileError) {
        console.error('Root entry is not found: ' +
            volumeMetadata.mountPath + ', ' + fileError.name);
        callback(new VolumeInfo(
            volumeMetadata.volumeType,
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
 * @type {Array.<util.VolumeType>}
 * @const
 * @private
 */
volumeManagerUtil.volumeListOrder_ = [
  util.VolumeType.DRIVE,
  util.VolumeType.DOWNLOADS,
  util.VolumeType.ARCHIVE,
  util.VolumeType.REMOVABLE,
  util.VolumeType.CLOUD_DEVICE
];

/**
 * Orders two volumes by volumeType and volumeId.
 *
 * The volumes at first are compared by volume type in the order of
 * volumeListOrder_.  Then they are compared by volume ID.
 *
 * @param {VolumeInfo} volumeInfo1 Volume info to be compared.
 * @param {VolumeInfo} volumeInfo2 Volume info to be compared.
 * @return {number} Returns -1 if volume1 < volume2, returns 1 if volume2 >
 *     volume1, returns 0 if volume1 === volume2.
 * @private
 */
volumeManagerUtil.compareVolumeInfo_ = function(volumeInfo1, volumeInfo2) {
  var typeIndex1 =
      volumeManagerUtil.volumeListOrder_.indexOf(volumeInfo1.volumeType);
  var typeIndex2 =
      volumeManagerUtil.volumeListOrder_.indexOf(volumeInfo2.volumeType);
  if (typeIndex1 !== typeIndex2)
    return typeIndex1 < typeIndex2 ? -1 : 1;
  if (volumeInfo1.volumeId !== volumeInfo2.volumeId)
    return volumeInfo1.volumeId < volumeInfo2.volumeId ? -1 : 1;
  return 0;
};

/**
 * The container of the VolumeInfo for each mounted volume.
 * @constructor
 */
function VolumeInfoList() {
  var field = 'volumeType,volumeId';

  /**
   * Holds VolumeInfo instances.
   * @type {cr.ui.ArrayDataModel}
   * @private
   */
  this.model_ = new cr.ui.ArrayDataModel([]);
  this.model_.setCompareFunction(field, volumeManagerUtil.compareVolumeInfo_);
  this.model_.sort(field, 'asc');

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
  var index = this.findIndex(volumeInfo.volumeId);
  if (index !== -1)
    this.model_.splice(index, 1, volumeInfo);
  else
    this.model_.push(volumeInfo);
};

/**
 * Removes the VolumeInfo having the given ID.
 * @param {string} volumeId ID of the volume.
 */
VolumeInfoList.prototype.remove = function(volumeId) {
  var index = this.findIndex(volumeId);
  if (index !== -1)
    this.model_.splice(index, 1);
};

/**
 * Obtains an index from the volume ID.
 * @param {string} volumeId Volume ID.
 * @return {number} Index of the volume.
 */
VolumeInfoList.prototype.findIndex = function(volumeId) {
  for (var i = 0; i < this.model_.length; i++) {
    if (this.model_.item(i).volumeId === volumeId)
      return i;
  }
  return -1;
};

/**
 * Searches the information of the volume that contains the passed entry.
 * @param {Entry|Object} entry Entry on the volume to be foudn.
 * @return {VolumeInfo} The volume's information, or null if not found.
 */
VolumeInfoList.prototype.findByEntry = function(entry) {
  // TODO(mtomasz): Switch to comparing file systems once possible.
  for (var i = 0; i < this.length; i++) {
    var volumeInfo = this.item(i);
    if (!volumeInfo.root)
      continue;
    // URL of the root always contains the trailing slash.
    if (util.isSameEntry(entry, volumeInfo.root) ||
        entry.toURL().indexOf(volumeInfo.root.toURL()) === 0) {
      return volumeInfo;
    }
    // Additionally, check fake entries.
    for (var key in volumeInfo.fakeEntries_) {
      var fakeEntry = volumeInfo.fakeEntries_[key];
      if (util.isSameEntry(fakeEntry, entry))
        return volumeInfo;
    }
  }
  return null;
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

  /**
   * Queue for mounting.
   * @type {AsyncUtil.Queue}
   * @private
   */
  this.mountQueue_ = new AsyncUtil.Queue();

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
    // We must subscribe to the mount completed event in the callback of
    // getVolumeMetadataList. crbug.com/330061.
    // But volumes reported by onMountCompleted events must be added after the
    // volumes in the volumeMetadataList are mounted. crbug.com/135477.
    this.mountQueue_.run(function(inCallback) {
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
      group.run(function() {
        // Call the callback of the initialize function.
        callback();
        // Call the callback of AsyncQueue. Maybe it invokes callbacks
        // registered by mountCompleted events.
        inCallback();
      });
    }.bind(this));

    chrome.fileBrowserPrivate.onMountCompleted.addListener(
        this.onMountCompleted_.bind(this));
  }.bind(this));
};

/**
 * Event handler called when some volume was mounted or unmounted.
 * @param {MountCompletedEvent} event Received event.
 * @private
 */
VolumeManager.prototype.onMountCompleted_ = function(event) {
  this.mountQueue_.run(function(callback) {
    if (event.eventType === 'mount') {
      var requestKey = this.makeRequestKey_(
          'mount',
          event.volumeMetadata.sourcePath);

      // TODO(mtomasz): Migrate to volumeId once possible.
      if (event.volumeMetadata.mountPath) {
        var error = event.status === 'success' ? '' : event.status;
        volumeManagerUtil.createVolumeInfo(
            event.volumeMetadata,
            function(volumeInfo) {
              this.volumeInfoList.add(volumeInfo);
              this.finishRequest_(requestKey, event.status, volumeInfo);
              if (volumeInfo.volumeType === util.VolumeType.DRIVE) {
                // Update the network connection status, because until the
                // drive is initialized, the status is set to not ready.
                // TODO(hidehiko): The connection status should be migrated into
                // VolumeMetadata.
                this.onDriveConnectionStatusChanged_();
              }
              callback();
            }.bind(this));
      } else {
        console.warn('No mount path.');
        this.finishRequest_(requestKey, event.status);
        callback();
      }
    } else if (event.eventType === 'unmount') {
      var volumeId = event.volumeMetadata.volumeId;
      var status = event.status;
      if (status === util.VolumeError.PATH_UNMOUNTED) {
        console.warn('Volume already unmounted: ', volumeId);
        status = 'success';
      }
      var requestKey = this.makeRequestKey_('unmount', volumeId);
      var requested = requestKey in this.requests_;
      var volumeInfoIndex =
          this.volumeInfoList.findIndex(volumeId);
      var volumeInfo = volumeInfoIndex !== -1 ?
          this.volumeInfoList.item(volumeInfoIndex) : null;
      if (event.status === 'success' && !requested && volumeInfo) {
        console.warn('Mounted volume without a request: ', volumeId);
        var e = new Event('externally-unmounted');
        e.volumeInfo = volumeInfo;
        this.dispatchEvent(e);
      }
      this.finishRequest_(requestKey, status);

      if (event.status === 'success')
        this.volumeInfoList.remove(event.volumeMetadata.volumeId);
      callback();
    }
  }.bind(this));
};

/**
 * Creates string to match mount events with requests.
 * @param {string} requestType 'mount' | 'unmount'. TODO(hidehiko): Replace by
 *     enum.
 * @param {string} argument Argument describing the request, eg. source file
 *     path of the archive to be mounted, or a volumeId for unmounting.
 * @return {string} Key for |this.requests_|.
 * @private
 */
VolumeManager.prototype.makeRequestKey_ = function(requestType, argument) {
  return requestType + ':' + argument;
};

/**
 * @param {string} fileUrl File url to the archive file.
 * @param {function(VolumeInfo)} successCallback Success callback.
 * @param {function(util.VolumeError)} errorCallback Error callback.
 */
VolumeManager.prototype.mountArchive = function(
    fileUrl, successCallback, errorCallback) {
  chrome.fileBrowserPrivate.addMount(fileUrl, function(sourcePath) {
    console.info(
        'Mount request: url=' + fileUrl + '; sourcePath=' + sourcePath);
    var requestKey = this.makeRequestKey_('mount', sourcePath);
    this.startRequest_(requestKey, successCallback, errorCallback);
  }.bind(this));
};

/**
 * Unmounts volume.
 * @param {!VolumeInfo} volumeInfo Volume to be unmounted.
 * @param {function()} successCallback Success callback.
 * @param {function(util.VolumeError)} errorCallback Error callback.
 */
VolumeManager.prototype.unmount = function(volumeInfo,
                                           successCallback,
                                           errorCallback) {
  chrome.fileBrowserPrivate.removeMount(volumeInfo.volumeId);
  var requestKey = this.makeRequestKey_('unmount', volumeInfo.volumeId);
  this.startRequest_(requestKey, successCallback, errorCallback);
};

/**
 * Obtains a volume info containing the passed entry.
 * @param {Entry|Object} entry Entry on the volume to be returned. Can be fake.
 * @return {VolumeInfo} The VolumeInfo instance or null if not found.
 */
VolumeManager.prototype.getVolumeInfo = function(entry) {
  return this.volumeInfoList.findByEntry(entry);
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
        true /* the entry points a root directory. */,
        true /* fake entries are read only. */);
  }

  var volumeInfo = this.volumeInfoList.findByEntry(entry);
  if (!volumeInfo)
    return null;

  var rootType;
  var isReadOnly;
  var isRootEntry;
  if (volumeInfo.volumeType === util.VolumeType.DRIVE) {
    // For Drive, the roots are /root and /other, instead of /. Root URLs
    // contain trailing slashes.
    // TODO(mtomasz): Simplify once switching to filesystem per volume.
    if (entry.toURL() === volumeInfo.root.toURL() + 'root' ||
        entry.toURL().indexOf(volumeInfo.root.toURL() + 'root/') === 0) {
      rootType = RootType.DRIVE;
      isReadOnly = volumeInfo.isReadOnly;
      isRootEntry = entry.toURL() === volumeInfo.root.toURL() + 'root';
    } else if (entry.toURL() === volumeInfo.root.toURL() + 'other' ||
        entry.toURL().indexOf(volumeInfo.root.toURL() + 'other/') === 0) {
      rootType = RootType.DRIVE_OTHER;
      isReadOnly = true;
      isRootEntry = entry.toURL() === volumeInfo.root.toURL() + 'other';
    } else {
      // Accessing Drive files outside of /drive/root and /drive/other is not
      // allowed, but can happen. Therefore returning null.
      return null;
    }
  } else {
    // Otherwise, root path is same with a mount path of the volume.
    switch (volumeInfo.volumeType) {
      case util.VolumeType.DOWNLOADS:
        rootType = RootType.DOWNLOADS;
        break;
      case util.VolumeType.REMOVABLE:
        rootType = RootType.REMOVABLE;
        break;
      case util.VolumeType.ARCHIVE:
        rootType = RootType.ARCHIVE;
        break;
      case util.VolumeType.CLOUD_DEVICE:
        rootType = RootType.CLOUD_DEVICE;
        break;
      default:
        // Programming error, throw an exception.
        throw new Error('Invalid volume type: ' + volumeInfo.volumeType);
    }
    isReadOnly = volumeInfo.isReadOnly;
    isRootEntry = util.isSameEntry(entry, volumeInfo.root);
  }

  return new EntryLocation(volumeInfo, rootType, isRootEntry, isReadOnly);
};

/**
 * @param {string} key Key produced by |makeRequestKey_|.
 * @param {function(VolumeInfo)} successCallback To be called when request
 *     finishes successfully.
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
 * @param {VolumeInfo=} opt_volumeInfo Volume info of the mounted volume.
 * @private
 */
VolumeManager.prototype.finishRequest_ = function(key, status, opt_volumeInfo) {
  var request = this.requests_[key];
  if (!request)
    return;

  clearTimeout(request.timeout);
  this.invokeRequestCallbacks_(request, status, opt_volumeInfo);
  delete this.requests_[key];
};

/**
 * @param {Object} request Structure created in |startRequest_|.
 * @param {util.VolumeError|string} status If status === 'success'
 *     success callbacks are called.
 * @param {VolumeInfo=} opt_volumeInfo Volume info of the mounted volume.
 * @private
 */
VolumeManager.prototype.invokeRequestCallbacks_ = function(
    request, status, opt_volumeInfo) {
  var callEach = function(callbacks, self, args) {
    for (var i = 0; i < callbacks.length; i++) {
      callbacks[i].apply(self, args);
    }
  };
  if (status === 'success') {
    callEach(request.successCallbacks, this, [opt_volumeInfo]);
  } else {
    volumeManagerUtil.validateError(status);
    callEach(request.errorCallbacks, this, [status]);
  }
};

/**
 * Location information which shows where the path points in FileManager's
 * file system.
 *
 * @param {!VolumeInfo} volumeInfo Volume information.
 * @param {RootType} rootType Root type.
 * @param {boolean} isRootEntry Whether the entry is root entry or not.
 * @param {boolean} isReadOnly Whether the entry is read only or not.
 * @constructor
 */
function EntryLocation(volumeInfo, rootType, isRootEntry, isReadOnly) {
  /**
   * Volume information.
   * @type {!VolumeInfo}
   */
  this.volumeInfo = volumeInfo;

  /**
   * Root type.
   * @type {RootType}
   */
  this.rootType = rootType;

  /**
   * Whether the entry is root entry or not.
   * @type {boolean}
   */
  this.isRootEntry = isRootEntry;

  /**
   * Whether the location obtained from the fake entry correspond to special
   * searches.
   * @type {boolean}
   */
  this.isSpecialSearchRoot =
      this.rootType === RootType.DRIVE_OFFLINE ||
      this.rootType === RootType.DRIVE_SHARED_WITH_ME ||
      this.rootType === RootType.DRIVE_RECENT;

  /**
   * Whether the location is under Google Drive or a special search root which
   * represents a special search from Google Drive.
   * @type {boolean}
   */
  this.isDriveBased =
      this.rootType === RootType.DRIVE ||
      this.rootType === RootType.DRIVE_OTHER ||
      this.rootType === RootType.DRIVE_SHARED_WITH_ME ||
      this.rootType === RootType.DRIVE_RECENT ||
      this.rootType === RootType.DRIVE_OFFLINE;

  /**
   * Whether the given path can be a target path of folder shortcut.
   * @type {boolean}
   */
  this.isEligibleForFolderShortcut =
      !this.isSpecialSearchRoot &&
      !this.isRootEntry &&
      this.isDriveBased;

  /**
   * Whether the entry is read only or not.
   * @type {boolean}
   */
  this.isReadOnly = isReadOnly;

  Object.freeze(this);
}
