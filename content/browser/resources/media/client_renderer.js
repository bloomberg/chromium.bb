// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ClientRenderer = (function() {
  var ClientRenderer = function() {
    this.playerListElement = document.getElementById('player-list');
    this.audioPropertiesTable =
        document.getElementById('audio-property-table').querySelector('tbody');
    this.playerPropertiesTable =
        document.getElementById('player-property-table').querySelector('tbody');
    this.logTable = document.getElementById('log').querySelector('tbody');
    this.graphElement = document.getElementById('graphs');
    this.audioPropertyName = document.getElementById('audio-property-name');

    this.players = null;
    this.selectedPlayer = null;
    this.selectedAudioComponentType = null;
    this.selectedAudioComponentId = null;
    this.selectedAudioCompontentData = null;

    this.selectedPlayerLogIndex = 0;

    this.filterFunction = function() { return true; };
    this.filterText = document.getElementById('filter-text');
    this.filterText.onkeyup = this.onTextChange_.bind(this);
    this.clipboardDialog = document.getElementById('clipboard-dialog');

    this.clipboardTextarea = document.getElementById('clipboard-textarea');
    this.clipboardTextarea.onblur = this.hideClipboard_.bind(this);
    var clipboardButtons = document.getElementsByClassName('copy-button');
    for (var i = 0; i < clipboardButtons.length; i++) {
      clipboardButtons[i].onclick = this.copyToClipboard_.bind(this);
    }

    this.saveLogButton = document.getElementById('save-log-button');
    this.saveLogButton.onclick = this.saveLog_.bind(this);

    this.hiddenKeys = ['component_id', 'component_type', 'owner_id'];

    // Tell CSS to hide certain content prior to making selections.
    document.body.classList.add(ClientRenderer.Css_.NO_PLAYERS_SELECTED);
    document.body.classList.add(ClientRenderer.Css_.NO_COMPONENTS_SELECTED);
  };

  /**
   * CSS classes added / removed in JS to trigger styling changes.
   * @private @enum {string}
   */
  ClientRenderer.Css_ = {
    NO_PLAYERS_SELECTED: 'no-players-selected',
    NO_COMPONENTS_SELECTED: 'no-components-selected',
    SELECTABLE_BUTTON: 'selectable-button'
  };

  function removeChildren(element) {
    while (element.hasChildNodes()) {
      element.removeChild(element.lastChild);
    }
  };

  function createSelectableButton(id, groupName, text, select_cb) {
    // For CSS styling.
    var radioButton = document.createElement('input');
    radioButton.classList.add(ClientRenderer.Css_.SELECTABLE_BUTTON);
    radioButton.type = 'radio';
    radioButton.id = id;
    radioButton.name = groupName;

    var buttonLabel = document.createElement('label');
    buttonLabel.classList.add(ClientRenderer.Css_.SELECTABLE_BUTTON);
    buttonLabel.setAttribute('for', radioButton.id);
    buttonLabel.appendChild(document.createTextNode(text));

    var fragment = document.createDocumentFragment();
    fragment.appendChild(radioButton);
    fragment.appendChild(buttonLabel);

    // Listen to 'change' rather than 'click' to keep styling in sync with
    // button behavior.
    radioButton.addEventListener('change', function() {
      select_cb();
    });

    return fragment;
  };

  function selectSelectableButton(id) {
    var element = document.getElementById(id);
    if (!element) {
      console.error('failed to select button with id: ' + id);
      return;
    }

    element.checked = true;
  }

  function downloadLog(text) {
    var file = new Blob([text], {type: 'text/plain'});
    var a = document.createElement('a');
    a.href = URL.createObjectURL(file);
    a.download = "media-internals.txt";
    a.click();
  }

  ClientRenderer.prototype = {
    /**
     * Called when an audio component is added to the collection.
     * @param componentType Integer AudioComponent enum value; must match values
     * from the AudioLogFactory::AudioComponent enum.
     * @param components The entire map of components (name -> dict).
     */
    audioComponentAdded: function(componentType, components) {
      this.redrawAudioComponentList_(componentType, components);

      // Redraw the component if it's currently selected.
      if (this.selectedAudioComponentType == componentType &&
          this.selectedAudioComponentId &&
          this.selectedAudioComponentId in components) {
        // TODO(chcunningham): This path is used both for adding and updating
        // the components. Split this up to have a separate update method.
        // At present, this selectAudioComponent call is key to *updating* the
        // the property table for existing audio components.
        this.selectAudioComponent_(
            componentType, this.selectedAudioComponentId,
            components[this.selectedAudioComponentId]);
      }
    },

    /**
     * Called when an audio component is removed from the collection.
     * @param componentType Integer AudioComponent enum value; must match values
     * from the AudioLogFactory::AudioComponent enum.
     * @param components The entire map of components (name -> dict).
     */
    audioComponentRemoved: function(componentType, components) {
      this.redrawAudioComponentList_(componentType, components);
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
     * Called when a player is removed from the collection.
     * @param players The entire map of id -> player.
     * @param playerRemoved The player that was removed.
     */
    playerRemoved: function(players, playerRemoved) {
      if (playerRemoved === this.selectedPlayer) {
        removeChildren(this.playerPropertiesTable);
        removeChildren(this.logTable);
        removeChildren(this.graphElement);
        document.body.classList.add(ClientRenderer.Css_.NO_PLAYERS_SELECTED);
      }
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
        this.drawProperties_(player.properties, this.playerPropertiesTable);
        this.drawLog_();
      }
      if (key === 'name' || key === 'url') {
        this.redrawPlayerList_(players);
      }
    },

    createVideoCaptureFormatTable: function(formats) {
      if (!formats || formats.length == 0)
        return document.createTextNode('No formats');

      var table = document.createElement('table');
      var thead = document.createElement('thead');
      var theadRow = document.createElement('tr');
      for (var key in formats[0]) {
        var th = document.createElement('th');
        th.appendChild(document.createTextNode(key));
        theadRow.appendChild(th);
      }
      thead.appendChild(theadRow);
      table.appendChild(thead);
      var tbody = document.createElement('tbody');
      for (var i=0; i < formats.length; ++i) {
        var tr = document.createElement('tr')
        for (var key in formats[i]) {
          var td = document.createElement('td');
          td.appendChild(document.createTextNode(formats[i][key]));
          tr.appendChild(td);
        }
        tbody.appendChild(tr);
      }
      table.appendChild(tbody);
      table.classList.add('video-capture-formats-table');
      return table;
    },

    redrawVideoCaptureCapabilities: function(videoCaptureCapabilities, keys) {
      var copyButtonElement =
          document.getElementById('video-capture-capabilities-copy-button');
      copyButtonElement.onclick = function() {
        this.showClipboard(JSON.stringify(videoCaptureCapabilities, null, 2));
      }.bind(this);

      var videoTableBodyElement  =
          document.getElementById('video-capture-capabilities-tbody');
      removeChildren(videoTableBodyElement);

      for (var component in videoCaptureCapabilities) {
        var tableRow =  document.createElement('tr');
        var device = videoCaptureCapabilities[ component ];
        for (var i in keys) {
          var value = device[keys[i]];
          var tableCell = document.createElement('td');
          var cellElement;
          if ((typeof value) == (typeof [])) {
            cellElement = this.createVideoCaptureFormatTable(value);
          } else {
            cellElement = document.createTextNode(
                ((typeof value) == 'undefined') ? 'n/a' : value);
          }
          tableCell.appendChild(cellElement)
          tableRow.appendChild(tableCell);
        }
        videoTableBodyElement.appendChild(tableRow);
      }
    },

    getAudioComponentName_: function(componentType, id) {
      var baseName;
      switch (componentType) {
        case 0:
        case 1:
          baseName = 'Controller';
          break;
        case 2:
          baseName = 'Stream';
          break;
        default:
          baseName = 'UnknownType'
          console.error('Unrecognized component type: ' + componentType);
          break;
      }
      return baseName + ' ' + id;
    },

    getListElementForAudioComponent_: function(componentType) {
      var listElement;
      switch (componentType) {
        case 0:
          listElement = document.getElementById(
              'audio-input-controller-list');
          break;
        case 1:
          listElement = document.getElementById(
              'audio-output-controller-list');
          break;
        case 2:
          listElement = document.getElementById(
              'audio-output-stream-list');
          break;
        default:
          console.error('Unrecognized component type: ' + componentType);
          listElement = null;
          break;
      }
      return listElement;
    },

    redrawAudioComponentList_: function(componentType, components) {
      // Group name imposes rule that only one component can be selected
      // (and have its properties displayed) at a time.
      var buttonGroupName = 'audio-components';

      var listElement = this.getListElementForAudioComponent_(componentType);
      if (!listElement) {
        console.error('Failed to find list element for component type: ' +
            componentType);
        return;
      }

      var fragment = document.createDocumentFragment();
      for (var id in components) {
        var li = document.createElement('li');
        var button_cb = this.selectAudioComponent_.bind(
                this, componentType, id, components[id]);
        var friendlyName = this.getAudioComponentName_(componentType, id);
        li.appendChild(createSelectableButton(
            id, buttonGroupName, friendlyName, button_cb));
        fragment.appendChild(li);
      }
      removeChildren(listElement);
      listElement.appendChild(fragment);

      if (this.selectedAudioComponentType &&
          this.selectedAudioComponentType == componentType &&
          this.selectedAudioComponentId in components) {
        // Re-select the selected component since the button was just recreated.
        selectSelectableButton(this.selectedAudioComponentId);
      }
    },

    selectAudioComponent_: function(componentType, componentId, componentData) {
      document.body.classList.remove(
         ClientRenderer.Css_.NO_COMPONENTS_SELECTED);

      this.selectedAudioComponentType = componentType;
      this.selectedAudioComponentId = componentId;
      this.selectedAudioCompontentData = componentData;
      this.drawProperties_(componentData, this.audioPropertiesTable);

      removeChildren(this.audioPropertyName);
      this.audioPropertyName.appendChild(document.createTextNode(
          this.getAudioComponentName_(componentType, componentId)));
    },

    redrawPlayerList_: function(players) {
      this.players = players;

      // Group name imposes rule that only one component can be selected
      // (and have its properties displayed) at a time.
      var buttonGroupName = 'player-buttons';

      var hasPlayers = false;
      var fragment = document.createDocumentFragment();
      for (var id in players) {
        hasPlayers = true;
        var player = players[id];
        var usableName = player.properties.name ||
            player.properties.url ||
            'Player ' + player.id;

        var li = document.createElement('li');
        var button_cb = this.selectPlayer_.bind(this, player);
        li.appendChild(createSelectableButton(
            id, buttonGroupName, usableName, button_cb));
        fragment.appendChild(li);
      }
      removeChildren(this.playerListElement);
      this.playerListElement.appendChild(fragment);

      if (this.selectedPlayer && this.selectedPlayer.id in players) {
        // Re-select the selected player since the button was just recreated.
        selectSelectableButton(this.selectedPlayer.id);
      }

      this.saveLogButton.style.display = hasPlayers ? "inline-block" : "none";
    },

    selectPlayer_: function(player) {
      document.body.classList.remove(ClientRenderer.Css_.NO_PLAYERS_SELECTED);

      this.selectedPlayer = player;
      this.selectedPlayerLogIndex = 0;
      this.selectedAudioComponentType = null;
      this.selectedAudioComponentId = null;
      this.selectedAudioCompontentData = null;
      this.drawProperties_(player.properties, this.playerPropertiesTable);

      removeChildren(this.logTable);
      removeChildren(this.graphElement);
      this.drawLog_();
    },

    drawProperties_: function(propertyMap, propertiesTable) {
      removeChildren(propertiesTable);
      var sortedKeys = Object.keys(propertyMap).sort();
      for (var i = 0; i < sortedKeys.length; ++i) {
        var key = sortedKeys[i];
        if (this.hiddenKeys.indexOf(key) >= 0)
          continue;

        var value = propertyMap[key];
        var row = propertiesTable.insertRow(-1);
        var keyCell = row.insertCell(-1);
        var valueCell = row.insertCell(-1);

        keyCell.appendChild(document.createTextNode(key));
        valueCell.appendChild(document.createTextNode(value));
      }
    },

    appendEventToLog_: function(event) {
      if (this.filterFunction(event.key)) {
        var row = this.logTable.insertRow(-1);

        var timestampCell = row.insertCell(-1);
        timestampCell.classList.add('timestamp');
        timestampCell.appendChild(document.createTextNode(
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

    saveLog_: function() {
      var strippedPlayers = []
      for (var id in this.players) {
        var p = this.players[id];
        strippedPlayers.push({properties: p.properties, events: p.allEvents});
      }
      downloadLog(JSON.stringify(strippedPlayers, null, 2));
    },

    showClipboard: function(string) {
      this.clipboardTextarea.value = string;
      this.clipboardDialog.showModal();
      this.clipboardTextarea.focus();
      this.clipboardTextarea.select();
    },

    hideClipboard_: function() {
      this.clipboardDialog.close();
    },

    copyToClipboard_: function() {
      if (!this.selectedPlayer && !this.selectedAudioCompontentData) {
        return;
      }
      var properties = this.selectedAudioCompontentData ||
          this.selectedPlayer.properties;
      var stringBuffer = [];

      for (var key in properties) {
        var value = properties[key];
        stringBuffer.push(key.toString());
        stringBuffer.push(': ');
        stringBuffer.push(value.toString());
        stringBuffer.push('\n');
      }

      this.showClipboard(stringBuffer.join(''));
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
        removeChildren(this.logTable);
        this.selectedPlayerLogIndex = 0;
      }
    },
  };

  return ClientRenderer;
})();
