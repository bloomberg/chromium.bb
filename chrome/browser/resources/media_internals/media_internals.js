// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="cache_entry.js"/>
<include src="disjoint_range_set.js"/>
<include src="event_list.js"/>
<include src="item_store.js"/>
<include src="media_player.js"/>
<include src="metrics.js"/>
<include src="util.js"/>

cr.define('media', function() {
  'use strict';

  // Stores information on open audio streams, referenced by id.
  var audioStreams = new media.ItemStore;

  // Active media players, indexed by 'render_id:player_id'.
  var mediaPlayers = {};

  // Cached files indexed by key and source id.
  var cacheEntriesByKey = {};
  var cacheEntries = {};

  // Map of event source -> url.
  var requestURLs = {};

  // Constants passed to us from Chrome.
  var eventTypes = {};
  var eventPhases = {};

  // The <div>s on the page in which to display information.
  var audioStreamDiv;
  var cacheDiv;

  // A timer used to limit the rate of redrawing the Media Players section.
  var redrawTimer = null;

  /**
   * Initialize variables and ask MediaInternals for all its data.
   */
  function initialize() {
    audioStreamDiv = $('audio-streams');
    cacheDiv = $('cache-entries');

    // Get information about all currently active media.
    chrome.send('getEverything');
  }

  /**
   * Write the set of audio streams to the DOM.
   */
  function printAudioStreams() {

    /**
     * Render a single stream as a <li>.
     * @param {Object} stream The stream to render.
     * @return {HTMLElement} A <li> containing the stream information.
     */
    function printStream(stream) {
      var out = document.createElement('li');
      out.id = stream.id;
      out.className = 'audio-stream';
      out.setAttribute('status', stream.status);

      out.textContent += 'Audio stream ' + stream.id.split('.')[1];
      out.textContent += ' is ' + (stream.playing ? 'playing' : 'paused');
      if (typeof stream.volume != 'undefined') {
        out.textContent += ' at ' + (stream.volume * 100).toFixed(0);
        out.textContent += '% volume.';
      }
      return out;
    }

    var out = document.createElement('ul');
    audioStreams.map(printStream).forEach(function(s) {
      out.appendChild(s);
    });

    audioStreamDiv.textContent = '';
    audioStreamDiv.appendChild(out);
  }

  /**
   * Redraw each MediaPlayer.
   */
  function printMediaPlayers() {
    for (var key in mediaPlayers) {
      mediaPlayers[key].redraw();
    }
    redrawTimer = null;
  }

  /**
   * Write the set of sparse CacheEntries to the DOM.
   */
  function printSparseCacheEntries() {
    var out = document.createElement('ul');
    for (var key in cacheEntriesByKey) {
      if (cacheEntriesByKey[key].sparse)
        out.appendChild(cacheEntriesByKey[key].toListItem());
    }

    cacheDiv.textContent = '';
    cacheDiv.appendChild(out);
  }

  /**
   * Receiving data for an audio stream.
   * Add it to audioStreams and update the page.
   * @param {Object} stream JSON representation of an audio stream.
   */
  function addAudioStream(stream) {
    audioStreams.addItem(stream);
    printAudioStreams();
  }

  /**
   * Receiving all data.
   * Add it all to the appropriate stores and update the page.
   * @param {Object} stuff JSON containing lists of data.
   * @param {Object} stuff.audio_streams A dictionary of audio streams.
   */
  function onReceiveEverything(stuff) {
    audioStreams.addItems(stuff.audio_streams);
    printAudioStreams();
  }

  /**
   * Removing an item from the appropriate store.
   * @param {string} id The id of the item to be removed, in the format
   *    "item_type.identifying_info".
   */
  function onItemDeleted(id) {
    var type = id.split('.')[0];
    switch (type) {
      case 'audio_streams':
        audioStreams.removeItem(id);
        printAudioStreams();
        break;
    }
  }

  /**
   * A render process has ended, delete any media players associated with it.
   * @param {number} renderer The id of the render process.
   */
  function onRendererTerminated(renderer) {
    for (var key in mediaPlayers) {
      if (mediaPlayers[key].renderer == renderer) {
        $('media-players').removeChild(mediaPlayers[key]);
        delete mediaPlayers[key];
        break;
      }
    }
    printMediaPlayers();
  }

  /**
   * Receiving net events.
   * Update cache information and update that section of the page.
   * @param {Array} updates A list of net events that have occurred.
   */
  function onNetUpdate(updates) {
    updates.forEach(function(update) {
      var id = update.source.id;
      if (!cacheEntries[id])
        cacheEntries[id] = new media.CacheEntry;

      switch (eventPhases[update.phase] + '.' + eventTypes[update.type]) {
      case 'PHASE_BEGIN.DISK_CACHE_ENTRY_IMPL':
        var key = update.params.key;

        // Merge this source with anything we already know about this key.
        if (cacheEntriesByKey[key]) {
          cacheEntriesByKey[key].merge(cacheEntries[id]);
          cacheEntries[id] = cacheEntriesByKey[key];
        } else {
          cacheEntriesByKey[key] = cacheEntries[id];
        }
        cacheEntriesByKey[key].key = key;
        break;

      case 'PHASE_BEGIN.SPARSE_READ':
        cacheEntries[id].readBytes(update.params.offset,
                                   update.params.buff_len);
        cacheEntries[id].sparse = true;
        break;

      case 'PHASE_BEGIN.SPARSE_WRITE':
        cacheEntries[id].writeBytes(update.params.offset,
                                    update.params.buff_len);
        cacheEntries[id].sparse = true;
        break;

      case 'PHASE_BEGIN.URL_REQUEST_START_JOB':
        requestURLs[update.source.id] = update.params.url;
        break;

      case 'PHASE_NONE.HTTP_TRANSACTION_READ_RESPONSE_HEADERS':
        // Record the total size of the file if this was a range request.
        var range = /content-range:\s*bytes\s*\d+-\d+\/(\d+)/i.exec(
            update.params.headers);
        var key = requestURLs[update.source.id];
        delete requestURLs[update.source.id];
        if (range && key) {
          if (!cacheEntriesByKey[key]) {
            cacheEntriesByKey[key] = new media.CacheEntry;
            cacheEntriesByKey[key].key = key;
          }
          cacheEntriesByKey[key].size = range[1];
        }
        break;
      }
    });

    printSparseCacheEntries();
  }

  /**
   * Receiving values for constants. Store them for later use.
   * @param {Object} constants A dictionary of constants.
   * @param {Object} constants.eventTypes A dictionary of event name -> int.
   * @param {Object} constants.eventPhases A dictionary of event phase -> int.
   */
  function onReceiveConstants(constants) {
    var events = constants.eventTypes;
    for (var e in events) {
      eventTypes[events[e]] = e;
    }

    var phases = constants.eventPhases;
    for (var p in phases) {
      eventPhases[phases[p]] = p;
    }
  }

  /**
   * Receiving notification of a media event.
   * @param {Object} event The json representation of a MediaLogEvent.
   */
  function onMediaEvent(event) {
    var source = event.renderer + ':' + event.player;
    var item = mediaPlayers[source] ||
        new media.MediaPlayer({id: source, renderer: event.renderer});
    mediaPlayers[source] = item;
    item.addEvent(event);

    // Both media and net events could provide the size of the file.
    // Media takes priority, but keep the size in both places synchronized.
    if (cacheEntriesByKey[item.properties.url]) {
      item.properties.total_bytes = item.properties.total_bytes ||
                                    cacheEntriesByKey[item.properties.url].size;
      cacheEntriesByKey[item.properties.url].size = item.properties.total_bytes;
    }

    // Events tend to arrive in groups; don't redraw the page too often.
    if (!redrawTimer)
      redrawTimer = setTimeout(printMediaPlayers, 50);
  }

  return {
    initialize: initialize,
    addAudioStream: addAudioStream,
    cacheEntriesByKey: cacheEntriesByKey,
    onReceiveEverything: onReceiveEverything,
    onItemDeleted: onItemDeleted,
    onRendererTerminated: onRendererTerminated,
    onNetUpdate: onNetUpdate,
    onReceiveConstants: onReceiveConstants,
    onMediaEvent: onMediaEvent
  };
});

/**
 * Initialize everything once we have access to the DOM.
 */
document.addEventListener('DOMContentLoaded', function() {
  media.initialize();
});
