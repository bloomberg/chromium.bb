// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A global object that gets used by the C++ interface.
 */
var media = (function() {
  'use strict';

  var manager_ = null;

  /**
   * Users of |media| must call initialize prior to calling other methods.
   */
  media.initialize = function(manager) {
    manager_ = manager;
  };

  media.onReceiveEverything = function(everything) {
    for (var key in everything.audio_streams) {
      media.updateAudioStream(everything.audio_streams[key]);
    }
  };

  media.onNetUpdate = function(update) {
    // TODO(tyoverby): Implement
  };

  media.onRendererTerminated = function(renderId) {
    util.object.forEach(manager_.players_, function(playerInfo, id) {
      if (playerInfo.properties['render_id'] == renderId) {
        manager_.removePlayer(id);
      }
    });
  };

  // For whatever reason, addAudioStream is also called on
  // the removal of audio streams.
  media.addAudioStream = function(event) {
    switch (event.status) {
      case 'created':
        manager_.addAudioStream(event.id);
        manager_.updateAudioStream(event.id, { 'playing': event.playing });
        break;
      case 'closed':
        manager_.removeAudioStream(event.id);
        break;
    }
  };

  media.updateAudioStream = function(stream) {
    manager_.addAudioStream(stream.id);
    manager_.updateAudioStream(stream.id, stream);
  };

  media.onItemDeleted = function() {
    // This only gets called when an audio stream is removed, which
    // for whatever reason is also handled by addAudioStream...
    // Because it is already handled, we can safely ignore it.
  };

  media.onPlayerOpen = function(id, timestamp) {
    manager_.addPlayer(id, timestamp);
  };

  media.onMediaEvent = function(event) {
    var source = event.renderer + ':' + event.player;

    // Although this gets called on every event, there is nothing we can do
    // because there is no onOpen event.
    media.onPlayerOpen(source);
    manager_.updatePlayerInfoNoRecord(
        source, event.ticksMillis, 'render_id', event.renderer);
    manager_.updatePlayerInfoNoRecord(
        source, event.ticksMillis, 'player_id', event.player);

    var propertyCount = 0;
    util.object.forEach(event.params, function(value, key) {
      key = key.trim();

      // These keys get spammed *a lot*, so put them on the display
      // but don't log list.
      if (key === 'buffer_start' ||
          key === 'buffer_end' ||
          key === 'buffer_current' ||
          key === 'is_downloading_data') {
        manager_.updatePlayerInfoNoRecord(
            source, event.ticksMillis, key, value);
      } else {
        manager_.updatePlayerInfo(source, event.ticksMillis, key, value);
      }
      propertyCount += 1;
    });

    if (propertyCount === 0) {
      manager_.updatePlayerInfo(
          source, event.ticksMillis, 'EVENT', event.type);
    }
  };

  return media;
}());
