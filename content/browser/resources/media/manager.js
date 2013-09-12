// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Keeps track of all the existing PlayerInfo and
 * audio stream objects and is the entry-point for messages from the backend.
 *
 * The events captured by Manager (add, remove, update) are relayed
 * to the clientRenderer which it can choose to use to modify the UI.
 */
var Manager = (function() {
  'use strict';

  function Manager(clientRenderer) {
    this.players_ = {};
    this.audioStreams_ = {};
    this.clientRenderer_ = clientRenderer;
  }

  Manager.prototype = {
    /**
     * Adds an audio-stream to the dictionary of audio-streams to manage.
     * @param id The unique-id of the audio-stream.
     */
    addAudioStream: function(id) {
      this.audioStreams_[id] = this.audioStreams_[id] || {};
      this.clientRenderer_.audioStreamAdded(this.audioStreams_,
                                            this.audioStreams_[id]);
    },

    /**
     * Sets properties of an audiostream.
     * @param id The unique-id of the audio-stream.
     * @param properties A dictionary of properties to be added to the
     * audio-stream.
     */
    updateAudioStream: function(id, properties) {
      for (var key in properties) {
        this.audioStreams_[id][key] = properties[key];
      }
      this.clientRenderer_.audioStreamAdded(
          this.audioStreams_, this.audioStreams_[id]);
    },

    /**
     * Removes an audio-stream from the manager.
     * @param id The unique-id of the audio-stream.
     */
    removeAudioStream: function(id) {
      this.clientRenderer_.audioStreamRemoved(
          this.audioStreams_, this.audioStreams_[id]);
      delete this.audioStreams_[id];
    },


    /**
     * Adds a player to the list of players to manage.
     */
    addPlayer: function(id) {
      if (this.players_[id]) {
        return;
      }
      // Make the PlayerProperty and add it to the mapping
      this.players_[id] = new PlayerInfo(id);
      this.clientRenderer_.playerAdded(this.players_, this.players_[id]);
    },

    /**
     * Attempts to remove a player from the UI.
     * @param id The ID of the player to remove.
     */
    removePlayer: function(id) {
      delete this.players_[id];
      this.clientRenderer_.playerRemoved(this.players_, this.players_[id]);
    },

    updatePlayerInfoNoRecord: function(id, timestamp, key, value) {
      if (!this.players_[id]) {
        console.error('[updatePlayerInfo] Id ' + id + ' does not exist');
        return;
      }

      this.players_[id].addPropertyNoRecord(timestamp, key, value);
      this.clientRenderer_.playerUpdated(this.players_,
                                         this.players_[id],
                                         key,
                                         value);
    },

    /**
     *
     * @param id The unique ID that identifies the player to be updated.
     * @param timestamp The timestamp of when the change occured.  This
     * timestamp is *not* normalized.
     * @param key The name of the property to be added/changed.
     * @param value The value of the property.
     */
    updatePlayerInfo: function(id, timestamp, key, value) {
      if (!this.players_[id]) {
        console.error('[updatePlayerInfo] Id ' + id + ' does not exist');
        return;
      }

      this.players_[id].addProperty(timestamp, key, value);
      this.clientRenderer_.playerUpdated(this.players_,
                                         this.players_[id],
                                         key,
                                         value);
    }
  };

  return Manager;
}());
