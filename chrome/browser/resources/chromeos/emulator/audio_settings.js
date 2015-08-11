// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {string} */ var AudioNodeType = {
  HEADPHONE: 'HEADPHONE',
  MIC: 'MIC',
  USB: 'USB',
  BLUETOOTH: 'BLUETOOTH',
  HDMI: 'HDMI',
  INTERNAL_SPEAKER: 'INTERNAL_SPEAKER',
  INTERNAL_MIC: 'INTERNAL_MIC',
  KEYBOARD_MIC: 'KEYBOARD_MIC',
  AOKR: 'AOKR',
  POST_MIX_LOOPBACK: 'POST_MIX_LOOPBACK',
  POST_DSP_LOOPBACK: 'POST_DSP_LOOPBACK',
  OTHER: 'OTHER',
  NONE: '',
};

/**
 * An audio node. Based on the struct AudioNode found in audio_node.h.
 * @constructor
 */
var AudioNode = function() {
  // Whether node will input or output audio.
  this.isInput = false;

  // Node ID. Set to arbitrary default.
  this.id = '30001';

  // Display name of the node. When this is empty, cras will automatically
  // use |this.deviceName| as the display name.
  this.name = '';

  // The text label of the selected node name.
  this.deviceName = 'New Device';

  // Based on the AudioNodeType enum.
  this.type = AudioNodeType.HEADPHONE;

  // Whether the node is active or not.
  this.active = false;

  // The time the node was plugged in (in seconds).
  this.pluggedTime = 0;
};

Polymer({
  is: 'audio-settings',

  properties: {
    /**
     * The title to be displayed in a heading element for the element.
     */
    title: {type: String},

    /**
     * A set of audio nodes.
     * @type !Array<!AudioNode>
     */
    nodes: {type: Array, value: function() { return []; }},

    /**
     * A set of options for the possible audio node types.
     * AudioNodeType |type| is based on the AudioType emumation.
     * @type {!Array<!{name: string, type: string}>}
     */
    nodeTypeOptions: {
      type: Array,
      value: function() {
        return [
          {name: 'Headphones', type: AudioNodeType.HEADPHONE},
          {name: 'Mic', type: AudioNodeType.MIC},
          {name: 'Usb', type: AudioNodeType.USB},
          {name: 'Bluetooth', type: AudioNodeType.BLUETOOTH},
          {name: 'HDMI', type: AudioNodeType.HDMI},
          {name: 'Internal Speaker', type: AudioNodeType.INTERNAL_SPEAKER},
          {name: 'Internal Mic', type: AudioNodeType.INTERNAL_MIC},
          {name: 'Keyboard Mic', type: AudioNodeType.KEYBOARD_MIC},
          {name: 'Aokr', type: AudioNodeType.AOKR},
          {name: 'Post Mix Loopback', type: AudioNodeType.POST_MIX_LOOPBACK},
          {name: 'Post Dsp Loopback', type: AudioNodeType.POST_DSP_LOOPBACK},
          {name: 'Other', type: AudioNodeType.OTHER}
        ];
      }
    },
  },

  ready: function() {
    this.title = 'Audio Settings';
  },

  /**
   * Adds a new node with default settings to the list of nodes.
   */
  appendNewNode: function() {
    this.push('nodes', new AudioNode());
  },

  /**
   * This adds or modifies an audio node to the AudioNodeList.
   * @param {model: {index: number}} e Event with a model containing
   *     the index in |nodes| to add.
   */
  insertAudioNode: function(e) {
    // Create a new audio node and add all the properties from |nodes[i]|.
    // Also, send the corresponding type value based on the name of |info.type|.
    var info = this.nodes[e.model.index];
    chrome.send('insertAudioNode', [info]);
  },

  /**
   * Removes the audio node with id |id|.
   * @param {model: {index: number}} e Event with a model containing
   *     the index in |nodes| to add.
   */
  removeAudioNode: function(e) {
    var info = this.nodes[e.model.index];
    chrome.send('removeAudioNode', [info.id]);
  },

  /**
   * Called by the WebUI which provides a list of nodes.
   * @param {!Array<!AudioNode>} nodeList A list of audio nodes.
   */
  updateAudioNodes: function(nodeList) {
    /** @type {!Array<!AudioNode>} */ var newNodeList = [];
    for (var i = 0; i < nodeList.length; ++i) {
      // Create a new audio node and add all the properties from |nodeList[i]|.
      var node = new AudioNode();
      Object.assign(node, nodeList[i]);
      newNodeList.push(node);
    }
    this.nodes = newNodeList;
  }
});
