// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Test API for Chrome OS Video Player and Audio Player.
 *
 * To test the Video Player open a tab with the URL:
 *   chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/video_player.html
 *
 * To test the Audio Player open a tab with the URL:
 *   chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/mediaplayer.html
 *
 */
var playerTestAPI = {

  /* Methods common for audio and video players */

  /**
   * Respond with the path to the current media source.
   */
  getSrc: function() {
    playerTestAPI.respond_(util.extractFilePath(playerTestAPI.getMedia_().src));
  },

  /**
   * Respond with a boolean value, true if the media is playing.
   */
  isPlaying: function() {
    playerTestAPI.respond_(playerTestAPI.getControls_().isPlaying());
  },

  /**
   * Play the media.
   */
  play: function() {
    playerTestAPI.getControls_().play();
  },

  /**
   * Pause the playback.
   */
  pause: function() {
    playerTestAPI.getControls_().pause();
  },

  /**
   * Respond with a number, duration of the media in seconds.
   */
  getDuration: function() {
    playerTestAPI.respond_(playerTestAPI.getMedia_().duration);
  },

  /**
   * Respond with a number, current media position in seconds.
   */
  getCurrentTime: function() {
    playerTestAPI.respond_(playerTestAPI.getMedia_().currentTime);
  },

  /**
   * Change media position.
   * @param {number} time Media positions.
   */
  seekTo: function(time) {
    playerTestAPI.getMedia_().currentTime = time;
  },

  /* Video player-specific methods.
   *
   * To test the video player open a tab with the url:
   * chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/mediaplayer.html
   *
   */

  /**
   * Load the specified file in the video player,
   * Starts playing immediately.
   * @param {string} filePath File path.
   */
  loadVideo: function(filePath) {
    var url = util.makeFilesystemUrl(filePath);
    location.href = location.origin + location.pathname + '?' + url;
    reload();
  },

  /**
   * Respond with a number, current volume [0..100].
   */
  getVolume: function() {
    playerTestAPI.respond_(playerTestAPI.getMedia_().volume * 100);
  },

  /**
   * Change volume.
   * @param {number} volume Volume [0..100].
   */
  setVolume: function(volume) {
    playerTestAPI.respond_(
        playerTestAPI.getControls_().onVolumeChange_(volume / 100));
  },

  /**
   * Respond with a boolean, true if the volume is muted.
   */
  isMuted: function() {
    playerTestAPI.respond_(playerTestAPI.getMedia_().volume == 0);
  },

  /**
   * Mute the volume. No-op if already muted.
   */
  mute: function() {
    if (playerTestAPI.getMedia_().volume != 0)
      playerTestAPI.getControls_().onSoundButtonClick_();
  },

  /**
   * Unmute the volume. No-op if not muted.
   */
  unmute: function() {
    if (playerTestAPI.getMedia_().volume == 0)
      playerTestAPI.getControls_().onSoundButtonClick_();
  },

  /* Audio player-specific methods. */

  /**
   * Load a group of tracks into the audio player.
   * Starts playing one of the tracks immediately.
   * @param {Array.<string>} filePaths Array of file paths.
   * @param {number} firstTrack Number of the file to play first (0-based).
   */
  loadAudio: function(filePaths, firstTrack) {
    AudioPlayer.instance.load({
      items: filePaths.map(util.makeFilesystemUrl),
      position: firstTrack
    });
  },

  /**
   * Respond with a current track number,
   */
  getTrackNumber: function() {
    playerTestAPI.respond_(AudioPlayer.instance.currentTrack_);
  },

  /**
   * Play the next track.
   */
  forward: function() {
    playerTestAPI.getControls_().onAdvanceClick_(true /* forward */);
  },

  /**
   * Go back. Will restart the current track if the current position is > 5 sec
   * or play the previous track otherwise.
   */
  back: function() {
    playerTestAPI.getControls_().onAdvanceClick_(false /* back */);
  },

  /* Utility methods */

  /**
   * @return {AudioControls|VideoControls} Media controls.
   * @private
   */
  getControls_: function() {
    return window.controls || window.AudioPlayer.instance.audioControls_;
  },

  /**
   * @return {HTMLVideoElement|HTMLAudioElement} Media element.
   * @private
   */
  getMedia_: function() {
    return playerTestAPI.getControls_().getMedia();
  },

  /**
   * @param {string|boolean|number} value Value to send back.
   * @private
   */
  respond_: function(value) {
    if (window.domAutomationController)
      window.domAutomationController.send(value);
    else
      console.log('playerTestAPI response: ' + value);
  }
};
