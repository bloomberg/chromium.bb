// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword', function() {
  'use strict';

  /**
   * Class used to manage speaker training. Starts a hotwording session
   * if training is on, and automatically restarts the detector when a
   * a hotword is triggered.
   * @param {!hotword.StateManager} stateManager
   * @constructor
   * @extends {hotword.BaseSessionManager}
   */
  function TrainingManager(stateManager) {
    /**
     * Chrome event listeners. Saved so that they can be de-registered when
     * hotwording is disabled.
     * @private
     */
    this.finalizedSpeakerModelListener_ =
        this.handleFinalizeSpeakerModel_.bind(this);

    hotword.BaseSessionManager.call(this,
                                    stateManager,
                                    hotword.constants.SessionSource.TRAINING);
  }

  /**
   * Handles a success event on mounting the file system event.
   * @param {FileSystem} fs The FileSystem object.
   * @private
   */
  TrainingManager.onRequestFileSystemSuccess_ = function(fs) {
    fs.root.getFile(hotword.constants.SPEAKER_MODEL_FILE_NAME, {create: false},
        TrainingManager.deleteFile_, TrainingManager.fileErrorHandler_);

    for (var i = 0; i < hotword.constants.NUM_TRAINING_UTTERANCES; ++i) {
      fs.root.getFile(hotword.constants.UTTERANCE_FILE_PREFIX + i +
          hotword.constants.UTTERANCE_FILE_EXTENSION,
          {create: false},
          TrainingManager.deleteFile_, TrainingManager.fileErrorHandler_);
    }
  };

  /**
   * Deletes a file.
   * @param {FileEntry} fileEntry The FileEntry object.
   * @private
   */
  TrainingManager.deleteFile_ = function(fileEntry) {
    if (fileEntry.isFile) {
      hotword.debug('File found: ' + fileEntry.fullPath);
      if (hotword.DEBUG || window.localStorage['hotword.DEBUG']) {
        fileEntry.getMetadata(function(md) {
          hotword.debug('File size: ' + md.size);
        });
      }
      fileEntry.remove(function() {
          hotword.debug('File removed: ' + fileEntry.fullPath);
      }, TrainingManager.fileErrorHandler_);
    }
  };

  /**
   * Handles a failure event on mounting the file system event.
   * @param {FileError} e The FileError object.
   * @private
   */
  TrainingManager.fileErrorHandler_ = function(e) {
      hotword.debug('File error: ' + e.code);
  };

  /**
   * Handles a request to delete the speaker model.
   */
  TrainingManager.handleDeleteSpeakerModel = function() {
    window.webkitRequestFileSystem(PERSISTENT,
        hotword.constants.FILE_SYSTEM_SIZE_BYTES,
        TrainingManager.onRequestFileSystemSuccess_,
        TrainingManager.fileErrorHandler_);
  };

  TrainingManager.prototype = {
    __proto__: hotword.BaseSessionManager.prototype,

    /** @override */
     enabled: function() {
       return this.stateManager.isTrainingEnabled();
     },

    /** @override */
    updateListeners: function() {
      hotword.BaseSessionManager.prototype.updateListeners.call(this);

      if (this.enabled()) {
        // Detect when the speaker model needs to be finalized.
        if (!chrome.hotwordPrivate.onFinalizeSpeakerModel.hasListener(
                this.finalizedSpeakerModelListener_)) {
          chrome.hotwordPrivate.onFinalizeSpeakerModel.addListener(
              this.finalizedSpeakerModelListener_);
        }
        this.startSession(hotword.constants.RecognizerStartMode.NEW_MODEL);
      } else {
        chrome.hotwordPrivate.onFinalizeSpeakerModel.removeListener(
            this.finalizedSpeakerModelListener_);
      }
    },

    /** @override */
    handleHotwordTrigger: function(log) {
      if (this.enabled()) {
        hotword.BaseSessionManager.prototype.handleHotwordTrigger.call(
            this, log);
        this.startSession(hotword.constants.RecognizerStartMode.ADAPT_MODEL);
      }
    },

    /** @override */
    startSession: function(opt_mode) {
      this.stateManager.startSession(
          this.sessionSource_,
          function() {
            chrome.hotwordPrivate.setHotwordSessionState(true, function() {});
          },
          this.handleHotwordTrigger.bind(this),
          this.handleSpeakerModelSaved_.bind(this),
          opt_mode);
    },

    /**
     * Handles a hotwordPrivate.onFinalizeSpeakerModel event.
     * @private
     */
    handleFinalizeSpeakerModel_: function() {
      if (this.enabled())
        this.stateManager.finalizeSpeakerModel();
    },

    /**
     * Handles a hotwordPrivate.onFinalizeSpeakerModel event.
     * @private
     */
    handleSpeakerModelSaved_: function() {
      if (this.enabled())
        chrome.hotwordPrivate.notifySpeakerModelSaved();
    },
  };

  return {
    TrainingManager: TrainingManager
  };
});
