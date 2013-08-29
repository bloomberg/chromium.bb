// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ClientRenderer = (function() {
  var ClientRenderer = function() {
    this.playerListElement = document.getElementById('player-list');
    this.audioStreamListElement = document.getElementById('audio-stream-list');
    this.propertiesTable = document.getElementById('property-table');
    this.logTable = document.getElementById('log');

    this.selectedPlayer = null;
    this.selectedStream = null;

    this.selectedPlayerLogIndex = 0;
  };

  function removeChildren(element) {
    while (element.hasChildNodes()) {
      element.removeChild(element.lastChild);
    }
  };

  function createButton(text, select_cb) {
    var button = document.createElement('button');

    button.appendChild(document.createTextNode(text));
    button.onclick = function() {
      select_cb();
    };

    return button;
  };

  ClientRenderer.prototype = {
    audioStreamAdded: function(audioStreams, audioStreamAdded) {
      this.redrawAudioStreamList_(audioStreams);
    },

    audioStreamUpdated: function(audioStreams, stream, key, value) {
      if (stream === this.selectedStream) {
        this.drawProperties_(stream);
      }
    },

    audioStreamRemoved: function(audioStreams, audioStreamRemoved) {
      this.redrawAudioStreamList_(audioStreams);
    },

    /**
     * Called when a player is added to the collection.
     * @param players The entire map of id -> player.
     * @param player_added The player that is added.
     */
    playerAdded: function(players, playerAdded) {
      this.redrawPlayerList_(players);
    },

    /**
     * Called when a playre is removed from the collection.
     * @param players The entire map of id -> player.
     * @param player_added The player that was removed.
     */
    playerRemoved: function(players, playerRemoved) {
      this.redrawPlayerList_(players);
    },

    /**
     * Called when a property on a player is changed.
     * @param players The entire map of id -> player.
     * @param player The player that had its property changed.
     * @param key The name of the property that was changed.
     * @param value The new value of the property.
     */
    playerUpdated: function(players, player, key, value) {
      if (player === this.selectedPlayer) {
        this.drawProperties_(player.properties);
        this.drawLog_();
      }
      if (key === 'name' || key === 'url') {
        this.redrawPlayerList_(players);
      }
    },

    redrawAudioStreamList_: function(streams) {
      removeChildren(this.audioStreamListElement);

      for (id in streams) {
        var li = document.createElement('li');
        li.appendChild(createButton(
            id, this.selectAudioStream_.bind(this, streams[id])));
        this.audioStreamListElement.appendChild(li);
      }
    },

    selectAudioStream_: function(audioStream) {
      this.selectedStream = audioStream;
      this.selectedPlayer = null;
      this.drawProperties_(audioStream);
      removeChildren(this.logTable.querySelector('tbody'));
    },

    redrawPlayerList_: function(players) {
      removeChildren(this.playerListElement);

      for (id in players) {
        var li = document.createElement('li');
        var player = players[id];
        var usableName = player.properties.name ||
            player.properties.url ||
            'player ' + player.id;

        li.appendChild(createButton(
            usableName, this.selectPlayer_.bind(this, player)));
        this.playerListElement.appendChild(li);
      }
    },

    selectPlayer_: function(player) {
      this.selectedPlayer = player;
      this.selectedPlayerLogIndex = 0;
      this.selectedStream = null;
      this.drawProperties_(player.properties);

      removeChildren(this.logTable.querySelector('tbody'));
      this.drawLog_();
    },

    drawProperties_: function(propertyMap) {
      removeChildren(this.propertiesTable);

      for (key in propertyMap) {
        var value = propertyMap[key];
        var row = this.propertiesTable.insertRow(-1);
        var keyCell = row.insertCell(-1);
        var valueCell = row.insertCell(-1);

        keyCell.appendChild(document.createTextNode(key));
        valueCell.appendChild(document.createTextNode(value));
      }
    },

    appendEventToLog_: function(event) {
      var row = this.logTable.querySelector('tbody').insertRow(-1);

      row.insertCell(-1).appendChild(document.createTextNode(
          util.millisecondsToString(event.time)));
      row.insertCell(-1).appendChild(document.createTextNode(event.key));
      row.insertCell(-1).appendChild(document.createTextNode(event.value));
    },

    drawLog_: function() {
      var toDraw = this.selectedPlayer.allEvents.slice(
          this.selectedPlayerLogIndex);
      toDraw.forEach(this.appendEventToLog_.bind(this));
      this.selectedPlayerLogIndex = this.selectedPlayer.allEvents.length;
    }
  };

  return ClientRenderer;
})();
