// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Keeps track of all the existing
 * PlayerProperty objects and is the entry-point for messages from the backend.
 */
var PlayerManager = (function() {
  'use strict';

  function PlayerManager(renderManager) {
    this.players_ = {};
    this.renderman_ = renderManager;
    renderManager.playerManager = this;

    this.shouldRemovePlayer_ = function() {
      // This is only temporary until we get the UI hooked up.
      return true;
    };
  }

  PlayerManager.prototype = {

    /**
     * Adds a player to the list of players to manage.
     */
    addPlayer: function(id) {
      if (this.players_[id]) {
        return;
      }
      // Make the PlayerProperty and add it to the mapping
      this.players_[id] = new PlayerInfo(id);

      this.renderman_.redrawList();
    },

    /**
     * Attempts to remove a player from the UI.
     * @param id The ID of the player to remove.
     */
    removePlayer: function(id) {
      // Look at the check box to see if we should actually
      // remove it from the UI
      if (this.shouldRemovePlayer_()) {
        delete this.players_[id];
        this.renderman_.redrawList();
      } else if (this.players_[id]) {
        // Set a property on it to be removed at a later time
        this.players_[id].toRemove = true;
      }
    },

    /**
     * Selects a player and displays it on the UI.
     * This method is called from the UI.
     * @param id The ID of the player to display.
     */
    selectPlayer: function(id) {
      if (!this.players_[id]) {
        throw new Error('[selectPlayer] Id ' + id + ' does not exist.');
      }

      this.renderman_.select(id);
    },

    updatePlayerInfoNoRecord: function(id, timestamp, key, value) {
      if (!this.players_[id]) {
        console.error('[updatePlayerInfo] Id ' + id +
          ' does not exist');
        return;
      }

      this.players_[id].addPropertyNoRecord(timestamp, key, value);

      // If we can potentially rename the player, do so.
      if (key === 'name' || key === 'url') {
        this.renderman_.redrawList();
      }

      this.renderman_.update();
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
        console.error('[updatePlayerInfo] Id ' + id +
          ' does not exist');
        return;
      }

      this.players_[id].addProperty(timestamp, key, value);

      // If we can potentially rename the player, do so.
      if (key === 'name' || key === 'url') {
        this.renderman_.redrawList();
      }

      this.renderman_.update();
    }
  };

  return PlayerManager;
}());
