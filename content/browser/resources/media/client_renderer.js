// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ClientRenderer = (function() {
  var ClientRenderer = function() {
    this.playerListElement = document.getElementById('player-list');
    this.audioStreamListElement = document.getElementById('audio-stream-list');
    this.propertiesTable = document.getElementById('property-table');
    this.logTable = document.getElementById('log');
    this.graphElement = document.getElementById('graphs');

    this.selectedPlayer = null;
    this.selectedStream = null;

    this.selectedPlayerLogIndex = 0;

    this.filterFunction = function() { return true; };
    this.filterText = document.getElementById('filter-text');
    this.filterText.onkeyup = this.onTextChange_.bind(this);

    this.bufferCanvas = document.createElement('canvas');
    this.bufferCanvas.width = media.BAR_WIDTH;
    this.bufferCanvas.height = media.BAR_HEIGHT;

    this.clipboardTextarea = document.getElementById('clipboard-textarea');
    this.clipboardButton = document.getElementById('copy-button');
    this.clipboardButton.onclick = this.copyToClipboard_.bind(this);
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
        this.drawGraphs_();
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
      removeChildren(this.graphElement);
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
      removeChildren(this.graphElement);
      this.drawLog_();
      this.drawGraphs_();
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
      if (this.filterFunction(event.key)) {
        var row = this.logTable.querySelector('tbody').insertRow(-1);

        row.insertCell(-1).appendChild(document.createTextNode(
            util.millisecondsToString(event.time)));
        row.insertCell(-1).appendChild(document.createTextNode(event.key));
        row.insertCell(-1).appendChild(document.createTextNode(event.value));
      }
    },

    drawLog_: function() {
      var toDraw = this.selectedPlayer.allEvents.slice(
          this.selectedPlayerLogIndex);
      toDraw.forEach(this.appendEventToLog_.bind(this));
      this.selectedPlayerLogIndex = this.selectedPlayer.allEvents.length;
    },

    drawGraphs_: function() {
      function addToGraphs(name, graph, graphElement) {
        var li = document.createElement('li');
        li.appendChild(graph);
        li.appendChild(document.createTextNode(name));
        graphElement.appendChild(li);
      }

      var url = this.selectedPlayer.properties.url;
      if (!url) {
        return;
      }

      var cache = media.cacheForUrl(url);

      var player = this.selectedPlayer;
      var props = player.properties;

      var cacheExists = false;
      var bufferExists = false;

      if (props['buffer_start'] !== undefined &&
          props['buffer_current'] !== undefined &&
          props['buffer_end'] !== undefined &&
          props['total_bytes'] !== undefined) {
        this.drawBufferGraph_(props['buffer_start'],
                              props['buffer_current'],
                              props['buffer_end'],
                              props['total_bytes']);
        bufferExists = true;
      }

      if (cache) {
        if (player.properties['total_bytes']) {
          cache.size = Number(player.properties['total_bytes']);
        }
        cache.generateDetails();
        cacheExists = true;

      }

      if (!this.graphElement.hasChildNodes()) {
        if (bufferExists) {
          addToGraphs('buffer', this.bufferCanvas, this.graphElement);
        }
        if (cacheExists) {
          addToGraphs('cache read', cache.readCanvas, this.graphElement);
          addToGraphs('cache write', cache.writeCanvas, this.graphElement);
        }
      }
    },

    drawBufferGraph_: function(start, current, end, size) {
      var ctx = this.bufferCanvas.getContext('2d');
      var width = this.bufferCanvas.width;
      var height = this.bufferCanvas.height;
      ctx.fillStyle = '#aaa';
      ctx.fillRect(0, 0, width, height);

      var scale_factor = width / size;
      var left = start * scale_factor;
      var middle = current * scale_factor;
      var right = end * scale_factor;

      ctx.fillStyle = '#a0a';
      ctx.fillRect(left, 0, middle - left, height);
      ctx.fillStyle = '#aa0';
      ctx.fillRect(middle, 0, right - middle, height);
    },

    copyToClipboard_: function() {
      var properties = this.selectedStream ||
          this.selectedPlayer.properties || false;
      if (!properties) {
        return;
      }
      var stringBuffer = [];

      for (var key in properties) {
        var value = properties[key];
        stringBuffer.push(key.toString());
        stringBuffer.push(': ');
        stringBuffer.push(value.toString());
        stringBuffer.push('\n');
      }

      this.clipboardTextarea.value = stringBuffer.join('');
      this.clipboardTextarea.classList.remove('hidden');
      this.clipboardTextarea.focus();
      this.clipboardTextarea.select();

      // The act of copying anything from the textarea gets canceled
      // if the element in question gets the class 'hidden' (which contains the
      // css property display:none) before the event is finished. For this, it
      // is necessary put the property setting on the event loop to be executed
      // after the copy has taken place.
      this.clipboardTextarea.oncopy = function(event) {
        setTimeout(function(element) {
          event.target.classList.add('hidden');
        }, 0);
      };
    },

    onTextChange_: function(event) {
      var text = this.filterText.value.toLowerCase();
      var parts = text.split(',').map(function(part) {
        return part.trim();
      }).filter(function(part) {
        return part.trim().length > 0;
      });

      this.filterFunction = function(text) {
        text = text.toLowerCase();
        return parts.length === 0 || parts.some(function(part) {
          return text.indexOf(part) != -1;
        });
      };

      if (this.selectedPlayer) {
        removeChildren(this.logTable.querySelector('tbody'));
        this.selectedPlayerLogIndex = 0;
        this.drawLog_();
      }
    },
  };

  return ClientRenderer;
})();
