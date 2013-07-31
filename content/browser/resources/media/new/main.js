// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A global object that gets used by the C++ interface.
 */
var media = (function() {
  'use strict';

  var manager = null;

  /**
   * Users of |media| must call initialize prior to calling other methods.
   */
  function initialize(playerManager) {
    manager = playerManager;
  }

  /**
   * Call to modify or add a system property.
   */
  function onSystemProperty(timestamp, key, value) {
    console.log('System properties not yet implemented');
  }

  /**
   * Call to modify or add a property on a player.
   */
  function onPlayerProperty(id, timestamp, key, value) {
    manager.updatePlayerInfo(id, timestamp, key, value);
  }

  function onPlayerPropertyNoRecord(id, timestamp, key, value) {
    manager.updatePlayerInfoNoRecord(id, timestamp, key, value);
  }

  /**
   * Call to add a player.
   */
  function onPlayerOpen(id, timestamp) {
    manager.addPlayer(id, timestamp);
  }

  /**
   * Call to remove a player.
   */
  function onPlayerClose(id) {
    manager.removePlayer(id);
  }

  var media = {
    onSystemProperty: onSystemProperty,
    onPlayerProperty: onPlayerProperty,
    onPlayerPropertyNoRecord: onPlayerPropertyNoRecord,
    onPlayerOpen: onPlayerOpen,
    onPlayerClose: onPlayerClose,

    initialize: initialize
  };

  // Everything beyond this point is for backwards compatibility reasons.
  // It will go away when the backend is updated.

  media.onNetUpdate = function(update) {
    // TODO(tyoverby): Implement
  };

  media.onRendererTerminated = function(renderId) {
    util.object.forEach(manager.players_, function(playerInfo, id) {
      if (playerInfo.properties['render_id'] == renderId) {
        media.onPlayerClose(id);
      }
    });
  };

  // For whatever reason, addAudioStream is also called on
  // the removal of audio streams.
  media.addAudioStream = function(event) {
    switch (event.status) {
      case 'created':
        media.onPlayerOpen(event.id);
        // We have to simulate the timestamp since it isn't provided to us.
        media.onPlayerProperty(
            event.id, (new Date()).getTime(), 'playing', event.playing);
        break;
      case 'closed':
        media.onPlayerClose(event.id);
        break;
    }
  };
  media.onItemDeleted = function() {
    // This only gets called when an audio stream is removed, which
    // for whatever reason is also handled by addAudioStream...
    // Because it is already handled, we can safely ignore it.
  };

  media.onMediaEvent = function(event) {
    var source = event.renderer + ':' + event.player;

    // Although this gets called on every event, there is nothing we can do
    // about this because there is no onOpen event.
    media.onPlayerOpen(source);
    media.onPlayerPropertyNoRecord(
        source, event.ticksMillis, 'render_id', event.renderer);
    media.onPlayerPropertyNoRecord(
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
        media.onPlayerPropertyNoRecord(
            source, event.ticksMillis, key, value);
      } else {
        media.onPlayerProperty(source, event.ticksMillis, key, value);
      }
      propertyCount += 1;
    });

    if (propertyCount === 0) {
      media.onPlayerProperty(
          source, event.ticksMillis, 'EVENT', event.type);
    }
  };

  return media;
}());
