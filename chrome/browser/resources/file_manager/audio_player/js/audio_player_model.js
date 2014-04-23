// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global) {
  'use strict';

  /**
   * List of values to be stored into the model.
   * @type {Object<string, *>}
   * @const
   */
  var VALUES = Object.freeze({
    shuffle: false,
    repeat: false,
    volume: 100,
    expanded: false,
  });

  /**
   * Prefix of the stored values in the storage.
   * @type {string}
   */
  var STORAGE_PREFIX = 'audioplayer-';

  /**
   * Save the values in the model into the storage.
   * @param {AudioPlayerModel} model The model.
   */
  function saveModel(model) {
    var objectToBeSaved = {};
    for (var key in VALUES) {
      objectToBeSaved[STORAGE_PREFIX + key] = model[key];
    }

    chrome.storage.local.set(objectToBeSaved);
  };

  /**
   * Load the values in the model from the storage.
   * @param {AudioPlayerModel} model The model.
   * @param {function()} callback Called when the load is completed.
   */
  function loadModel(model, callback) {
    // Restores the values from the storage
    var objectsToBeRead = Object.keys(VALUES).
                          map(function(a) {
                            return STORAGE_PREFIX + a;
                          });

    chrome.storage.local.get(objectsToBeRead, function(result) {
      for (var key in result) {
        // Strips the prefix.
        model[key.substr(STORAGE_PREFIX.length)] = result[key];
      }
      callback();
    }.bind(this));
  };

  /**
   * The model class for audio player.
   * @constructor
   */
  function AudioPlayerModel() {
    // Initializes values.
    for (var key in VALUES) {
      this[key] = VALUES[key];
    }
    Object.seal(this);

    // Restores the values from the storage
    loadModel(this, function() {
      // Installs observer to watch changes of the values.
      var observer = new ObjectObserver(this);
      observer.open(function(added, removed, changed, getOldValueFn) {
        saveModel(this);
      }.bind(this));
    }.bind(this));
  }

  // Exports AudioPlayerModel class to the global.
  global.AudioPlayerModel = AudioPlayerModel;

})(this || window);
