// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ClientRenderer = (function() {
  var ClientRenderer = function() {
    this.playerListElement = document.getElementById('player-list');
    this.propertiesTable = document.getElementById('property-table');
    this.selectedPlayer = null;
  };

  function removeChildren(element) {
    while (element.hasChildNodes()) {
      element.removeChild(element.lastChild);
    }
  };

  ClientRenderer.prototype = {
    /**
     * Called when a player is added to the collection.
     * @param players The entire map of id -> player.
     * @param player_added The player that is added.
     */
    playerAdded: function(players, player_added) {
      this.redrawList_(players);
    },

    /**
     * Called when a playre is removed from the collection.
     * @param players The entire map of id -> player.
     * @param player_added The player that was removed.
     */
    playerRemoved: function(players, player_removed) {
      this.redrawList_(players);
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
        this.drawSelectedPlayer_();
      }
      if (key === 'name' || key === 'url') {
        this.redrawList_(players);
      }
    },

    redrawList_: function(players) {
      removeChildren(this.playerListElement);

      function createButton(player, select_cb) {
        var button = document.createElement('button');
        var usableName = player.properties.name ||
            player.properties.url ||
            'player ' + player.id;

        button.appendChild(document.createTextNode(usableName));
        button.onclick = function() {
          select_cb(player);
        };

        return button;
      };

      for (id in players) {
        var li = document.createElement('li');
        li.appendChild(createButton(players[id], this.select_.bind(this)));
        this.playerListElement.appendChild(li);
      }
    },

    select_: function(player) {
      this.selectedPlayer = player;
      this.drawSelectedPlayer_();
    },

    drawSelectedPlayer_: function() {
      removeChildren(this.propertiesTable);

      for (key in this.selectedPlayer.properties) {
        var value = this.selectedPlayer.properties[key];
        var row = this.propertiesTable.insertRow(-1);
        var keyCell = row.insertCell(-1);
        var valueCell = row.insertCell(-1);

        keyCell.appendChild(document.createTextNode(key));
        valueCell.appendChild(document.createTextNode(value));
      }
    }
  };

  return ClientRenderer;
})();
