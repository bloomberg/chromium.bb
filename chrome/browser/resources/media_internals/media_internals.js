// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('media', function() {

  // Stores information on open audio streams, referenced by id.
  var audioStreams = new media.ItemStore;
  // The <div> on the page in which to display audio stream data.
  var audioStreamDiv;

  /**
   * Initialize variables and ask MediaInternals for all its data.
   */
  initialize = function() {
    audioStreamDiv = document.getElementById('audio-streams');
    // Get information about all currently active media.
    chrome.send('getEverything');
  };

  /**
   * Write the set of audio streams to the DOM.
   */
  printAudioStreams = function() {

    /**
     * Render a single stream as a <li>.
     * @param {Object} stream The stream to render.
     * @return {HTMLElement} A <li> containing the stream information.
     */
    printStream = function(stream) {
      var out = document.createElement('li');
      out.id = stream.id;
      out.className = 'audio-stream';
      out.setAttribute('status', stream.status);

      out.innerHTML += 'Audio stream ' + stream.id.split('.')[1];
      out.innerHTML += ' is ' + (stream.playing ? 'playing' : 'paused');
      out.innerHTML += ' at ' + Math.round(stream.volume * 100) + '% volume.';
      return out;
    };

    var out = document.createElement('ul');
    audioStreams.map(printStream).forEach(function(s) {
      out.appendChild(s)
    });

    audioStreamDiv.textContent = '';
    audioStreamDiv.appendChild(out);
  };

  /**
   * Receiving data for an audio stream.
   * Add it to audioStreams and update the page.
   * @param {Object} stream JSON representation of an audio stream.
   */
  addAudioStream = function(stream) {
    audioStreams.addItem(stream);
    printAudioStreams();
  };

  /**
   * Receiving all data.
   * Add it all to the appropriate stores and update the page.
   * @param {Object} stuff JSON containing lists of data.
   * @param {Object} stuff.audio_streams A dictionary of audio streams.
   */
  onReceiveEverything = function(stuff) {
    audioStreams.addItems(stuff.audio_streams);
    printAudioStreams();
  };

  /**
   * Removing an item from the appropriate store.
   * @param {string} id The id of the item to be removed, in the format
   *    "item_type.identifying_info".
   */
  onItemDeleted = function(id) {
    var type = id.split('.')[0];
    switch (type) {
    case 'audio_streams':
      audioStreams.removeItem(id);
      printAudioStreams();
      break;
    }
  };

  return {
    initialize: initialize,
    addAudioStream: addAudioStream,
    onReceiveEverything: onReceiveEverything,
    onItemDeleted: onItemDeleted,
  };
});

/**
 * Initialize everything once we have access to the DOM.
 */
window.onload = function() {
  media.initialize();
};
