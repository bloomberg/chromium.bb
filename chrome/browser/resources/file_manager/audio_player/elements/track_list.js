// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  Polymer('track-list', {
    /**
     * Initializes an element. This method is called automatically when the
     * element is ready.
     */
    ready: function() {
      this.tracksObserver_ = new ArrayObserver(
          this.tracks,
          this.tracksValueChanged_.bind(this));
    },

    /**
     * List of tracks.
     * @type {Array.<AudioPlayer.TrackInfo>}
     */
    tracks: [],

    /**
     * Track index of the current track.
     * If the tracks propertye is empty, it should be -1. Otherwise, be a valid
     * track number.
     *
     * @type {number}
     */
    currentTrackIndex: -1,

    /**
     * Invoked when the current track index is changed.
     * @param {number} oldValue old value.
     * @param {number} newValue new value.
     */
    currentTrackIndexChanged: function(oldValue, newValue) {
      if (oldValue === newValue)
        return;

      if (oldValue !== -1)
        this.tracks[oldValue].active = false;

      if (newValue < 0 || this.tracks.length <= newValue) {
        if (this.tracks.length === 0)
          this.currentTrackIndex = -1;
        else
          this.currentTrackIndex = 0;
      } else {
        this.tracks[newValue].active = true;
      }
    },

    /**
     * Invoked when 'tracks' property is clicked.
     * @param {Event} event Click event.
     */
    tracksChanged: function(oldValue, newValue) {
      if (oldValue !== newValue) {
        this.tracksObserver_.close();
        this.tracksObserver_ = new ArrayObserver(
            this.tracks,
            this.tracksValueChanged_.bind(this));
        if (this.tracks.length !== 0)
          this.currentTrackIndex = 0;
      }

      if (this.tracks.length === 0)
        this.currentTrackIndex = -1;
    },

    /**
     * Invoked when the value in the 'tracks' is changed.
     * @param {Array.<Object>} splices The detail of the change.
     */
    tracksValueChanged_: function(splices) {
      if (this.tracks.length === 0)
        this.currentTrackIndex = -1;
      else
        this.tracks[this.currentTrackIndex].active = true;
    },

    /**
     * Invoked when the track element is clicked.
     * @param {Event} event Click event.
     */
    trackClicked: function(event) {
      var track = event.target.templateInstance.model;
      this.selectTrack(track);
    },

    /**
     * Sets the current track.
     * @param {AudioPlayer.TrackInfo} track TrackInfo to be set as the current
     *     track.
     */
    selectTrack: function(track) {
      var index = -1;
      for (var i = 0; i < this.tracks.length; i++) {
        if (this.tracks[i].url === track.url) {
          index = i;
          break;
        }
      }
      if (index >= 0)
        this.currentTrackIndex = index;
    },

    /**
     * Returns the current track.
     * @param {AudioPlayer.TrackInfo} track TrackInfo of the current track.
     */
    getCurrentTrack: function() {
      if (this.tracks.length === 0)
        return null;

      return this.tracks[this.currentTrackIndex];
    },

    /**
     * Returns the next (or previous) track in the track list.
     *
     * @param {boolean} forward Specify direction: forward or previous mode.
     *     True: forward mode, false: previous mode.
     * @return {AudioPlayer.TrackInfo} TrackInfo of the next track. If there is
     *     no track, the return value is null.
     */
    getNextTrackIndex: function(forward) {
      var defaultTrack = forward ? 0 : (this.tracks.length - 1);
      var tentativeNewTrackIndex = this.currentTrackIndex + (forward ? +1 : -1);
      var newTrackIndex;

      if (this.tracks.length === 0) {
        newTrackIndex = -1;
      } else {
        if (this.currentTrackIndex === -1) {
          newTrackIndex = defaultTrack;
        } else if (0 <= tentativeNewTrackIndex &&
                   tentativeNewTrackIndex < this.tracks.length) {
          newTrackIndex = tentativeNewTrackIndex;
        } else {
          newTrackIndex = defaultTrack;
        }
      }

      return newTrackIndex;
    },

    /**
     * Returns if the next (or previous) track in the track list is available.
     *
     * @param {boolean} forward Specify direction: forward or previous mode.
     *     True: forward mode, false: previous mode.
     * @return {true} True if the next (or previous) track available. False
     *     otherwise.
     */
    isNextTrackAvailable: function(forward) {
      if (this.tracks.length === 0) {
        return false;
      } else {
        var tentativeNewTrackIndex =
            this.currentTrackIndex + (forward ? +1 : -1);

        if (this.currentTrackIndex === -1) {
          return false;
        } else if (0 <= tentativeNewTrackIndex &&
                   tentativeNewTrackIndex < this.tracks.length) {
          return true;
        } else {
          return false;
        }
      }
    }
  });  // Polymer('track-list') block
})();  // Anonymous closure
