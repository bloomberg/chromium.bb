// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * The main namespace for the extension.
 * @namespace
 */
unpacker.app = {
  /**
   * The key used by chrome.storage.local to save and restore the volumes state.
   * @const {string}
   */
  STORAGE_KEY: 'state',

  /**
   * The default id for the NaCl module.
   * @const {string}
   */
  DEFAULT_MODULE_ID: 'nacl_module',

  /**
   * Time in milliseconds before the notification about mounting is shown.
   * @const {number}
   */
  MOUNTING_NOTIFICATION_DELAY: 1000,

  /**
   * Time in milliseconds before the notification about packing is shown.
   * @const {number}
   */
  PACKING_NOTIFICATION_DELAY: 1000,

  /**
   * Time in milliseconds before the notification is cleared.
   * @const {number}
   */
  PACKING_NOTIFICATION_CLEAR_DELAY: 1000,

  /**
   * The default filename for .nmf file.
   * This value must not be constant because it is overwritten in tests.
   * Since .nmf file is not available in .grd, we use .txt instead.
   * @type {string}
   */
  DEFAULT_MODULE_NMF: 'module.nmf',

  /**
   * The default MIME type for .nmf file.
   * @const {string}
   */
  DEFAULT_MODULE_TYPE: 'application/x-pnacl',

  /**
   * Multiple volumes can be opened at the same time.
   * @type {!Object<!unpacker.types.FileSystemId, !unpacker.Volume>}
   */
  volumes: {},

  /**
   * Each "Pack with" request from Files app creates a new compressor.
   * Thus, multiple compressors can exist at the same time.
   * @type {!Object<!unpacker.types.CompressorId, !unpacker.Compressor>}
   */
  compressors: {},

  /**
   * A promise used to postpone all calls to access string assets.
   * @type {?Promise<!Object>}
   */
  stringDataLoadedPromise: null,

  /**
   * A map with promises of loading a volume's metadata from NaCl.
   * Any call from fileSystemProvider API should work only on valid metadata.
   * These promises ensure that the fileSystemProvider API calls wait for the
   * metatada load.
   * @type {!Object<!unpacker.types.FileSystemId, !Promise>}
   */
  volumeLoadedPromises: {},

  /**
   * A Promise used to postpone all calls to fileSystemProvider API after
   * the NaCl module loads.
   * @type {?Promise}
   */
  moduleLoadedPromise: null,

  /**
   * The NaCl module containing the logic for decompressing archives.
   * @type {?Object}
   */
  naclModule: null,

  /**
   * The number of mount process in progress. NaCL module is unloaded if
   * app.unpacker.volumes is empty and this counter is 0. This is incremented
   * when a mounting process starts and decremented when it ends.
   * @type {number}
   */
  mountProcessCounter: 0,

  /**
   * Function called on receiving a message from NaCl module. Registered by
   * common.js.
   * Process pack message by getting compressor and passing the message to it.
   * @param {!Object} message The message received from NaCl module.
   * @param {!unpacker.request.Operation} operation
   * @private
   */
  handlePackMessage_: function(message, operation) {
    var compressorId = message.data[unpacker.request.Key.COMPRESSOR_ID];
    console.assert(compressorId, 'No NaCl compressor id.');

    var compressor = unpacker.app.compressors[compressorId];
    if (!compressor) {
      console.error('No compressor for compressor id: ' + compressorId + '.');
      return;
    }
    compressor.processMessage(message.data, operation);
  },

  /**
   * Process unpack message by getting volume and passing the message to it.
   * @param {!Object} message The message received from NaCl module.
   * @param {!unpacker.request.Operation} operation
   * @private
   */
  handleUnpackMessage_: function(message, operation) {
    var fileSystemId = message.data[unpacker.request.Key.FILE_SYSTEM_ID];
    console.assert(fileSystemId, 'No NaCl file system id.');

    var requestId = message.data[unpacker.request.Key.REQUEST_ID];
    console.assert(!!requestId, 'No NaCl request id.');

    var volume = unpacker.app.volumes[fileSystemId];
    if (!volume) {
      // The volume is gone, which can happen.
      console.info('No volume for: ' + fileSystemId + '.');
      return;
    }

    volume.decompressor.processMessage(
        message.data, operation, Number(requestId));
  },

  /**
   * Function called on receiving a message from NaCl module. Registered by
   * common.js.
   * @param {!Object} message The message received from NaCl module.
   * @private
   */
  handleMessage_: function(message) {
    // Get mandatory fields in a message.
    var operation = message.data[unpacker.request.Key.OPERATION];
    console.assert(
        operation != undefined,  // Operation can be 0.
        'No NaCl operation: ' + operation + '.');

    // Assign the message to either module.
    if (unpacker.request.isPackRequest(operation))
      unpacker.app.handlePackMessage_(message, operation);
    else
      unpacker.app.handleUnpackMessage_(message, operation);
  },

  /**
   * Saves state in case of restarts, event page suspend, crashes, etc. This
   * method does nothing when context is in incognito mode.
   * @param {!Array<!unpacker.types.FileSystemId>} fileSystemIdsArray
   * @private
   */
  saveState_: function(fileSystemIdsArray) {
    // If current context is in incognito mode, then skip save state because
    // retainEntry is not available in incognito mode.
    if (chrome.extension.inIncognitoContext)
      return;

    chrome.storage.local.get([unpacker.app.STORAGE_KEY], function(result) {
      if (!result[unpacker.app.STORAGE_KEY])  // First save state call.
        result[unpacker.app.STORAGE_KEY] = {};

      // Overwrite state only for the volumes that have their file system id
      // present in the input array. Leave the rest of the volumes state
      // untouched.
      fileSystemIdsArray.forEach(function(fileSystemId) {
        var entryId = chrome.fileSystem.retainEntry(
            unpacker.app.volumes[fileSystemId].entry);
        result[unpacker.app.STORAGE_KEY][fileSystemId] = {
          entryId: entryId,
          passphrase: unpacker.app.volumes[fileSystemId]
                          .decompressor.passphraseManager.rememberedPassphrase
        };
      });

      chrome.storage.local.set(result);
    });
  },

  /**
   * Removes state from local storage for a single volume. This method does
   * nothing when context is in incognito mode.
   * @param {!unpacker.types.FileSystemId} fileSystemId
   */
  removeState_: function(fileSystemId) {
    if (chrome.extension.inIncognitoContext)
      return;

    chrome.storage.local.get([unpacker.app.STORAGE_KEY], function(result) {
      console.assert(
          result[unpacker.app.STORAGE_KEY] &&
              result[unpacker.app.STORAGE_KEY][fileSystemId],
          'Should call removeState_ only for file systems that ',
          'have previously called saveState_.');

      delete result[unpacker.app.STORAGE_KEY][fileSystemId];
      chrome.storage.local.set(result);
    });
  },

  /**
   * Restores archive's entry and opened files for the passed file system id.
   * @param {!unpacker.types.FileSystemId} fileSystemId
   * @return {!Promise<!Object>} Promise fulfilled with the entry and list of
   *     opened files.
   * @private
   */
  restoreVolumeState_: function(fileSystemId) {
    return new Promise(function(fulfill, reject) {
      chrome.storage.local.get([unpacker.app.STORAGE_KEY], function(result) {
        if (!result[unpacker.app.STORAGE_KEY]) {
          reject('FAILED');
          return;
        }

        var volumeState = result[unpacker.app.STORAGE_KEY][fileSystemId];
        if (!volumeState) {
          console.error('No state for: ' + fileSystemId + '.');
          reject('FAILED');
          return;
        }

        chrome.fileSystem.restoreEntry(volumeState.entryId, function(entry) {
          if (chrome.runtime.lastError) {
            console.error(
                'Restore entry error for <', fileSystemId,
                '>: ' + chrome.runtime.lastError.message);
            reject('FAILED');
            return;
          }
          fulfill({entry: entry, passphrase: volumeState.passphrase});
        });
      });
    });
  },

  /**
   * Creates a volume and loads its metadata from NaCl.
   * @param {!unpacker.types.FileSystemId} fileSystemId
   * @param {!Entry} entry The volume's archive entry.
   * @param {!Object<!unpacker.types.RequestId,
   *                 !unpacker.types.OpenFileRequestedOptions>}
   *     openedFiles Previously opened files before a suspend.
   * @param {?string} passphrase Previously used passphrase before a suspend.
   * @return {!Promise} Promise fulfilled on success and rejected on failure.
   * @private
   */
  loadVolume_: function(fileSystemId, entry, openedFiles, passphrase) {
    return new Promise(function(fulfill, reject) {
      entry.file(
          function(file) {
            // File is a Blob object, so it's ok to construct the Decompressor
            // directly with it.
            var passphraseManager = new unpacker.PassphraseManager(passphrase);
            console.assert(
                unpacker.app.naclModule,
                'The NaCL module should have already been defined.');
            var decompressor = new unpacker.Decompressor(
                /** @type {!Object} */ (unpacker.app.naclModule), fileSystemId,
                file, passphraseManager);
            var volume = new unpacker.Volume(decompressor, entry);

            var onLoadVolumeSuccess = function() {
              if (Object.keys(openedFiles).length == 0) {
                fulfill();
                return;
              }

              // Restore opened files on NaCl side.
              var openFilePromises = [];
              for (var key in openedFiles) {
                // 'key' is always a number but JS compiler complains that it is
                // a string.
                var openRequestId = Number(key);
                var options =
                    /** @type {!unpacker.types.OpenFileRequestedOptions} */
                    (openedFiles[openRequestId]);
                openFilePromises.push(new Promise(function(resolve, reject) {
                  volume.onOpenFileRequested(options, resolve, reject);
                }));
              }

              Promise.all(openFilePromises).then(fulfill, reject);
            };

            unpacker.app.volumes[fileSystemId] = volume;
            volume.initialize(onLoadVolumeSuccess, reject);
          },
          function(error) {
            reject('FAILED');
          });
    });
  },

  /**
   * Restores a volume mounted previously to a suspend / restart. In case of
   * failure of the load promise for fileSystemId, the corresponding volume is
   * forcely unmounted.
   * @param {!unpacker.types.FileSystemId} fileSystemId
   * @return {!Promise} A promise that restores state and loads volume.
   * @private
   */
  restoreSingleVolume_: function(fileSystemId) {
    // Load volume after restart / suspend page event.
    return unpacker.app.restoreVolumeState_(fileSystemId)
        .then(function(state) {
          return new Promise(function(fulfill, reject) {
            // Check if the file system is compatible with this version of the
            // ZIP unpacker.
            // TODO(mtomasz): Implement remounting instead of unmounting.
            chrome.fileSystemProvider.get(fileSystemId, function(fileSystem) {
              if (chrome.runtime.lastError) {
                console.error(chrome.runtime.lastError.name);
                reject('FAILED');
                return;
              }
              if (!fileSystem || fileSystem.openedFilesLimit != 1) {
                console.error('No compatible mounted file system found.');
                reject('FAILED');
                return;
              }
              fulfill({state: state, fileSystem: fileSystem});
            });
          });
        })
        .then(function(stateWithFileSystem) {
          var openedFilesOptions = {};
          stateWithFileSystem.fileSystem.openedFiles.forEach(function(
              openedFile) {
            openedFilesOptions[openedFile.openRequestId] = {
              fileSystemId: fileSystemId,
              requestId: openedFile.openRequestId,
              mode: openedFile.mode,
              filePath: openedFile.filePath
            };
          });
          return unpacker.app.loadVolume_(
              fileSystemId, stateWithFileSystem.state.entry, openedFilesOptions,
              stateWithFileSystem.state.passphrase);
        })
        .catch(function(error) {
          console.error(error.stack || error);
          // Force unmount in case restore failed. All resources related to the
          // volume will be cleanup from both memory and local storage.
          // TODO(523195): Show a notification that the source file is gone.
          return unpacker.app.unmountVolume(fileSystemId, true)
              .then(function() {
                return Promise.reject('FAILED');
              });
        });
  },

  /**
   * Ensures a volume is loaded by returning its corresponding loaded promise
   * from unpacker.app.volumeLoadedPromises. In case there is no such promise,
   * then this is a call after suspend / restart and a new volume loaded promise
   * that restores state is returned.
   * @param {!unpacker.types.FileSystemId} fileSystemId
   * @return {!Promise} The loading volume promise.
   * @private
   */
  ensureVolumeLoaded_: function(fileSystemId) {
    // Increment the counter so that the NaCl module won't be unloaded until
    // the mounting process ends.
    unpacker.app.mountProcessCounter++;
    // Create a promise to load the NaCL module.
    if (!unpacker.app.moduleLoadedPromise)
      unpacker.app.loadNaclModule(
          unpacker.app.DEFAULT_MODULE_NMF, unpacker.app.DEFAULT_MODULE_TYPE);

    return unpacker.app.moduleLoadedPromise.then(function() {
      // In case there is no volume promise for fileSystemId then we
      // received a call after restart / suspend as load promises are
      // created on launched. In this case we will restore volume state
      // from local storage and create a new load promise.
      if (!unpacker.app.volumeLoadedPromises[fileSystemId]) {
        unpacker.app.volumeLoadedPromises[fileSystemId] =
            unpacker.app.restoreSingleVolume_(fileSystemId);
      }

      // Decrement the counter when the mounting process ends.
      unpacker.app.volumeLoadedPromises[fileSystemId]
          .then(function() {
            unpacker.app.mountProcessCounter--;
          })
          .catch(function(error) {
            unpacker.app.mountProcessCounter--;
          });

      return unpacker.app.volumeLoadedPromises[fileSystemId];
    });
  },

  /**
   * @return {boolean} True if NaCl module is loaded.
   */
  naclModuleIsLoaded: function() {
    return !!unpacker.app.naclModule;
  },

  /**
   * Loads string assets.
   */
  loadStringData: function() {
    unpacker.app.stringDataLoadedPromise = new Promise(function(fulfill) {
      chrome.fileManagerPrivate.getStrings(function(strings) {
        fulfill(strings);
      });
    });
  },

  /**
   * Loads the NaCl module.
   * @param {string} pathToConfigureFile Path to the module's configuration
   *     file, which should be a .nmf file.
   * @param {string} mimeType The mime type for the NaCl executable.
   * @param {string=} opt_moduleId The NaCl module id. Necessary for testing
   *     purposes.
   */
  loadNaclModule: function(pathToConfigureFile, mimeType, opt_moduleId) {
    unpacker.app.moduleLoadedPromise = new Promise(function(fulfill) {
      var moduleId =
          opt_moduleId ? opt_moduleId : unpacker.app.DEFAULT_MODULE_ID;
      var elementDiv = document.createElement('div');

      // Promise fulfills only after NaCl module has been loaded.
      elementDiv.addEventListener('load', function() {
        // Since the first load of the NaCL module is slow, the module is loaded
        // once in background.js in advance. If there is no mounted volume and
        // ongoing mounting process, the module is just unloaded. This is the
        // workaround for crbug.com/699930.
        if (Object.keys(unpacker.app.volumes).length === 0 &&
            unpacker.app.mountProcessCounter === 0) {
          elementDiv.parentNode.removeChild(elementDiv);
          // This is necessary for tests.
          fulfill();
          unpacker.app.moduleLoadedPromise = null;
          return;
        }
        unpacker.app.naclModule = document.querySelector('#' + moduleId);
        fulfill();
      }, true);

      elementDiv.addEventListener('message', unpacker.app.handleMessage_, true);

      var elementEmbed = document.createElement('embed');
      elementEmbed.id = moduleId;
      elementEmbed.style.width = 0;
      elementEmbed.style.height = 0;
      elementEmbed.src = pathToConfigureFile;
      elementEmbed.type = mimeType;
      elementDiv.appendChild(elementEmbed);

      document.body.appendChild(elementDiv);
      // Request the offsetTop property to force a relayout. As of Apr 10, 2014
      // this is needed if the module is being loaded on a Chrome App's
      // background page (see crbug.com/350445).
      /** @suppress {suspiciousCode} */ elementEmbed.offsetTop;
    });
  },

  /**
   * Unloads the NaCl module.
   */
  unloadNaclModule: function() {
    var naclModuleParentNode = unpacker.app.naclModule.parentNode;
    naclModuleParentNode.parentNode.removeChild(naclModuleParentNode);
    unpacker.app.naclModule = null;
    unpacker.app.moduleLoadedPromise = null;
  },

  /**
   * Cleans up the resources for a volume, except for the local storage. If
   * necessary that can be done using unpacker.app.removeState_.
   * @param {!unpacker.types.FileSystemId} fileSystemId
   */
  cleanupVolume: function(fileSystemId) {
    delete unpacker.app.volumes[fileSystemId];
    // Allow mount after clean.
    delete unpacker.app.volumeLoadedPromises[fileSystemId];

    if (Object.keys(unpacker.app.volumes).length === 0 &&
        unpacker.app.mountProcessCounter === 0) {
      unpacker.app.unloadNaclModule();
    } else {
      unpacker.app.naclModule.postMessage(
          unpacker.request.createCloseVolumeRequest(fileSystemId));
    }
  },

  /**
   * Cleans up the resources for a compressor.
   * @param {!unpacker.types.CompressorId} compressorId
   * @param {boolean} hasError
   */
  cleanupCompressor: function(compressorId, hasError) {
    var compressor = unpacker.app.compressors[compressorId];
    if (!compressor) {
      console.error('No compressor for: compressor id' + compressorId + '.');
      return;
    }

    unpacker.app.mountProcessCounter--;
    if (Object.keys(unpacker.app.volumes).length === 0 &&
        unpacker.app.mountProcessCounter === 0) {
      unpacker.app.unloadNaclModule();
    } else {
      // Request minizip to abort any ongoing process and release resources.
      // The argument indicates whether an error occurred or not.
      if (hasError)
        compressor.sendCloseArchiveRequest(hasError);
    }

    // Delete the archive file if it exists.
    if (compressor.archiveFileEntry)
      compressor.archiveFileEntry.remove();

    delete unpacker.app.compressors[compressorId];
  },

  /**
   * Updates the state in case of restarts, event page suspend, crashes, etc.
   * Use this method to update or save the state out side of the object in case
   * when password changes, etc.
   * @param {!Array<!unpacker.types.FileSystemId>} fileSystemIdsArray
   */
  updateState: function(fileSystemIdsArray) {
    unpacker.app.saveState_(fileSystemIdsArray);
  },

  /**
   * Unmounts a volume and removes any resources related to the volume from both
   * the extension and the local storage state.
   * @param {!unpacker.types.FileSystemId} fileSystemId
   * @param {boolean=} opt_forceUnmount True if unmount should be forced even if
   *     volume might be in use, or is not restored yet.
   * @return {!Promise} A promise that fulfills if volume is unmounted or
   *     rejects with ProviderError in case of any errors.
   */
  unmountVolume: function(fileSystemId, opt_forceUnmount) {
    return new Promise(function(fulfill, reject) {
      var volume = unpacker.app.volumes[fileSystemId];
      console.assert(
          volume || opt_forceUnmount,
          'Unmount that is not forced must not be called for ',
          'volumes that are not restored.');

      if (!opt_forceUnmount && volume.inUse()) {
        reject('IN_USE');
        return;
      }

      var options = {fileSystemId: fileSystemId};
      chrome.fileSystemProvider.unmount(options, function() {
        if (chrome.runtime.lastError) {
          console.error(
              'Unmount error: ' + chrome.runtime.lastError.message + '.');
          reject('FAILED');
          return;
        }

        // In case of forced unmount volume can be undefined due to not being
        // restored. An unmount that is not forced will be called only after
        // restoring state. In the case of forced unmount when volume is not
        // restored, we will not do a normal cleanup, but just remove the load
        // volume promise to allow further mounts.
        if (opt_forceUnmount)
          delete unpacker.app.volumeLoadedPromises[fileSystemId];
        else
          unpacker.app.cleanupVolume(fileSystemId);

        // Remove volume from local storage.
        unpacker.app.removeState_(fileSystemId);
        fulfill();
      });
    });
  },

  /**
   * Handles an unmount request received from File System Provider API.
   * @param {!unpacker.types.UnmountRequestedOptions} options
   * @param {function()} onSuccess Callback to execute on success.
   * @param {function(!ProviderError)} onError Callback to execute on error.
   */
  onUnmountRequested: function(options, onSuccess, onError) {
    unpacker.app.ensureVolumeLoaded_(options.fileSystemId)
        .then(function() {
          return unpacker.app.unmountVolume(options.fileSystemId);
        })
        .then(onSuccess)
        .catch(/** @type {function(*)} */ (onError));
  },

  /**
   * Obtains metadata about a file system entry.
   * @param {!unpacker.types.GetMetadataRequestedOptions} options
   * @param {function(!EntryMetadata)} onSuccess Callback to execute on success.
   *     The parameter is the EntryMetadata obtained by this function.
   * @param {function(!ProviderError)} onError Callback to execute on error.
   */
  onGetMetadataRequested: function(options, onSuccess, onError) {
    unpacker.app.ensureVolumeLoaded_(options.fileSystemId)
        .then(function() {
          unpacker.app.volumes[options.fileSystemId].onGetMetadataRequested(
              options, onSuccess, onError);
        })
        .catch(/** @type {function(*)} */ (onError));
  },

  /**
   * Reads a directory entries.
   * @param {!unpacker.types.ReadDirectoryRequestedOptions} options
   * @param {function(!Array<!EntryMetadata>, boolean)} onSuccess Callback to
   *     execute on success. The first parameter is an array with directory
   *     entries. The second parameter is 'hasMore', and if it's set to true,
   *     then onSuccess must be called again with the next directory entries.
   * @param {function(!ProviderError)} onError Callback to execute on error.
   */
  onReadDirectoryRequested: function(options, onSuccess, onError) {
    unpacker.app.ensureVolumeLoaded_(options.fileSystemId)
        .then(function() {
          unpacker.app.volumes[options.fileSystemId].onReadDirectoryRequested(
              options, onSuccess, onError);
        })
        .catch(/** @type {function(*)} */ (onError));
  },

  /**
   * Opens a file for read or write.
   * @param {!unpacker.types.OpenFileRequestedOptions} options
   * @param {function()} onSuccess Callback to execute on success.
   * @param {function(!ProviderError)} onError Callback to execute on error.
   */
  onOpenFileRequested: function(options, onSuccess, onError) {
    unpacker.app.ensureVolumeLoaded_(options.fileSystemId)
        .then(function() {
          unpacker.app.volumes[options.fileSystemId].onOpenFileRequested(
              options, onSuccess, onError);
        })
        .catch(/** @type {function(*)} */ (onError));
  },

  /**
   * Closes a file identified by options.openRequestId.
   * @param {!unpacker.types.CloseFileRequestedOptions} options
   * @param {function()} onSuccess Callback to execute on success.
   * @param {function(!ProviderError)} onError Callback to execute on error.
   */
  onCloseFileRequested: function(options, onSuccess, onError) {
    unpacker.app.ensureVolumeLoaded_(options.fileSystemId)
        .then(function() {
          unpacker.app.volumes[options.fileSystemId].onCloseFileRequested(
              options, onSuccess, onError);
        })
        .catch(/** @type {function(*)} */ (onError));
  },

  /**
   * Reads the contents of a file identified by options.openRequestId.
   * @param {!unpacker.types.ReadFileRequestedOptions} options
   * @param {function(!ArrayBuffer, boolean)} onSuccess Callback to execute on
   *     success. The first parameter is the read data and the second parameter
   *     is 'hasMore'. If it's set to true, then onSuccess must be called again
   *     with the next data to read.
   * @param {function(!ProviderError)} onError Callback to execute on error.
   */
  onReadFileRequested: function(options, onSuccess, onError) {
    unpacker.app.ensureVolumeLoaded_(options.fileSystemId)
        .then(function() {
          unpacker.app.volumes[options.fileSystemId].onReadFileRequested(
              options, onSuccess, onError);
        })
        .catch(/** @type {function(*)} */ (onError));
  },

  /**
   * Creates a new compressor and compresses entries.
   * @param {!Object} launchData
   */
  onLaunchedWithPack: function(launchData) {
    unpacker.app.mountProcessCounter++;

    // Create a promise to load the NaCL module.
    if (!unpacker.app.moduleLoadedPromise) {
      unpacker.app.loadNaclModule(
          unpacker.app.DEFAULT_MODULE_NMF, unpacker.app.DEFAULT_MODULE_TYPE);
    }

    unpacker.app.moduleLoadedPromise
        .then(function() {
          return unpacker.app.stringDataLoadedPromise;
        })
        .then(function(stringData) {
          var compressor = new unpacker.Compressor(
              /** @type {!Object} */ (unpacker.app.naclModule),
              launchData.items);

          var compressorId = compressor.getCompressorId();
          unpacker.app.compressors[compressorId] = compressor;

          // If packing takes significant amount of time, then show a
          // notification about packing in progress.
          var deferredNotificationTimer = setTimeout(function() {
            chrome.notifications.create(
                compressorId.toString(), {
                  type: 'basic',
                  iconUrl: chrome.runtime.getManifest().icons[128],
                  title: compressor.getArchiveName(),
                  message: stringData['ZIP_ARCHIVER_PACKING_DEFERRED_MESSAGE'],
                },
                function() {});
          }, unpacker.app.PACKING_NOTIFICATION_DELAY);

          var onError = function(compressorId) {
            clearTimeout(deferredNotificationTimer);
            chrome.notifications.create(
                compressorId.toString(), {
                  type: 'basic',
                  iconUrl: chrome.runtime.getManifest().icons[128],
                  title: compressor.getArchiveName(),
                  message: stringData['ZIP_ARCHIVER_PACKING_ERROR_MESSAGE']
                },
                function() {});
            unpacker.app.cleanupCompressor(compressorId, true /* hasError */);
          };

          var onSuccess = function(compressorId) {
            clearTimeout(deferredNotificationTimer);

            // Here we clear the notification with a delay because in case when
            // content of a zip file is small it will flash the notification.
            // Thus we clear the notification with a delay to avoid flashing.
            setTimeout(function() {
              chrome.notifications.clear(
                  compressorId.toString(), function() {});
            }, unpacker.app.PACKING_NOTIFICATION_CLEAR_DELAY);
            unpacker.app.cleanupCompressor(compressorId, false /* hasError */);
          };

          var progressNotificationCreated = false;
          var progressValue = -1;
          var onProgress = function(compressorId, progress) {
            clearTimeout(deferredNotificationTimer);
            progress = Math.round(progress * 100);

            // Check if value has changed at leats for 1 to not update the
            // notification for no reason.
            if (progressValue === progress)
              return;

            // TODO(tetsui): Check if icon on progress notification message is
            // visisble when bug related to the progress notification gets
            // resolved.
            if (!progressNotificationCreated) {
              chrome.notifications.create(
                  compressorId.toString(), {
                    type: 'progress',
                    iconUrl: chrome.runtime.getManifest().icons[128],
                    title: compressor.getArchiveName(),
                    message:
                        stringData['ZIP_ARCHIVER_PACKING_PROGRESS_MESSAGE'],
                    progress: progress
                  },
                  function() {});
              progressNotificationCreated = true;
            } else {
              chrome.notifications.update(
                  compressorId.toString(), {progress: progress});
            }

            progressValue = progress;
          };

          compressor.compress(onSuccess, onError, onProgress);

          // If notification is closed while packing is in progress, flag to
          // create/update is reset.
          chrome.notifications.onClosed.addListener(function() {
            progressNotificationCreated = false;
          });
        });
  },

  /**
   * Creates a volume for every opened file with the extension or mime type
   * declared in the manifest file.
   * @param {!Object} launchData
   * @param {function(string)=} opt_onSuccess Callback to execute in case a
   *     volume was loaded successfully. Has one parameter, which is the file
   *     system id of the loaded volume. Can be called multiple times, depending
   *     on how many volumes must be loaded.
   * @param {function(string)=} opt_onError Callback to execute in case of
   *     failure when loading a volume. Has one parameter, which is the file
   *     system id of the volume that failed to load. Can be called multiple
   *     times, depending on how many volumes must be loaded.
   */
  onLaunchedWithUnpack: function(launchData, opt_onSuccess, opt_onError) {
    // Increment the counter that indicates the number of ongoing mouot process.
    unpacker.app.mountProcessCounter++;

    // Create a promise to load the NaCL module.
    if (!unpacker.app.moduleLoadedPromise) {
      unpacker.app.loadNaclModule(
          unpacker.app.DEFAULT_MODULE_NMF, unpacker.app.DEFAULT_MODULE_TYPE);
    }

    unpacker.app.moduleLoadedPromise
        .then(function() {
          return unpacker.app.stringDataLoadedPromise;
        })
        .then(function(stringData) {
          unpacker.app.mountProcessCounter--;
          launchData.items.forEach(function(item) {
            unpacker.app.mountProcessCounter++;
            chrome.fileSystem.getDisplayPath(
                item.entry, function(entry, fileSystemId) {
                  // If loading takes significant amount of time, then show a
                  // notification about scanning in progress.
                  var deferredNotificationTimer = setTimeout(function() {
                    chrome.notifications.create(
                        fileSystemId, {
                          type: 'basic',
                          iconUrl: chrome.runtime.getManifest().icons[128],
                          title: entry.name,
                          message: stringData['ZIP_ARCHIVER_MOUNTING_MESSAGE'],
                        },
                        function() {});
                  }, unpacker.app.MOUNTING_NOTIFICATION_DELAY);

                  var onError = function(error, fileSystemId) {
                    clearTimeout(deferredNotificationTimer);
                    console.error('Mount error: ' + error.message + '.');
                    // Decrement the counter that indicates the number of
                    // ongoing mount process.
                    unpacker.app.mountProcessCounter--;
                    if (error.message === 'EXISTS') {
                      if (opt_onError)
                        opt_onError(fileSystemId);
                      return;
                    }
                    chrome.notifications.create(
                        fileSystemId, {
                          type: 'basic',
                          iconUrl: chrome.runtime.getManifest().icons[128],
                          title: entry.name,
                          message:
                              stringData['ZIP_ARCHIVER_OTHER_ERROR_MESSAGE'],
                        },
                        function() {});
                    if (opt_onError)
                      opt_onError(fileSystemId);
                    // Cleanup volume resources in order to allow future
                    // attempts to mount the volume. The volume can't be cleaned
                    // up in case of 'EXIST' because we should not clean the
                    // other already mounted volume.
                    unpacker.app.cleanupVolume(fileSystemId);
                  };

                  var onSuccess = function(fileSystemId) {
                    clearTimeout(deferredNotificationTimer);
                    chrome.notifications.clear(fileSystemId, function() {});
                    // Decrement the counter that indicates the number of
                    // ongoing mount process.
                    unpacker.app.mountProcessCounter--;
                    if (opt_onSuccess)
                      opt_onSuccess(fileSystemId);
                  };

                  var loadPromise = unpacker.app.loadVolume_(
                      fileSystemId, entry, {}, null /* passphrase */);
                  loadPromise
                      .then(function() {
                        // Mount the volume and save its information in local
                        // storage in order to be able to recover the metadata
                        // in case of restarts, system crashes, etc.
                        chrome.fileSystemProvider.mount(
                            {
                              fileSystemId: fileSystemId,
                              displayName: entry.name,
                              openedFilesLimit: 1
                            },
                            function() {
                              if (chrome.runtime.lastError) {
                                onError(chrome.runtime.lastError, fileSystemId);
                                return;
                              }
                              // Save state so in case of restarts we are able
                              // to correctly get the archive's metadata.
                              unpacker.app.saveState_([fileSystemId]);
                              onSuccess(fileSystemId);
                            });
                      })
                      .catch(function(error) {
                        onError(error.stack || error, fileSystemId);
                        return Promise.reject(error);
                      });

                  unpacker.app.volumeLoadedPromises[fileSystemId] = loadPromise;
                }.bind(null, item.entry));
          });
        })
        .catch(function(error) {
          console.error(error.stack || error);
        });
  },

  /**
   * Fired when this extension is launched.
   * Calls a module designated by launchData.id.
   * Currently, Verbs API does not support "unpack" option. Thus, any launchData
   * that does not have "pack" as id is regarded as unpack for now.
   * @param {!Object} launchData
   * @param {function(string)=} opt_onSuccess
   * @param {function(string)=} opt_onError
   */
  onLaunched: function(launchData, opt_onSuccess, opt_onError) {
    if (launchData.items == null) {
      // The user tried to launch us directly.
      console.log('Ignoring launch request w/out items field', {launchData});
      return;
    }

    if (launchData.id === 'pack')
      unpacker.app.onLaunchedWithPack(launchData);
    else
      unpacker.app.onLaunchedWithUnpack(launchData, opt_onSuccess, opt_onError);
  },

  /**
   * Saves the state before suspending the event page, so we can resume it
   * once new events arrive.
   */
  onSuspend: function() {
    unpacker.app.saveState_(Object.keys(unpacker.app.volumes));
  }
};
