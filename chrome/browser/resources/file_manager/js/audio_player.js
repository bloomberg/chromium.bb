// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.addEventListener('DOMContentLoaded', function() {
  // Test harness sets the search string to prevent the automatic load.
  // It calls AudioPlayer.load() explicitly after initializing
  // the |chrome| variable with an appropriate mock object.
  if (!document.location.search) {
    AudioPlayer.load();
  }
});

/**
 * @param {HTMLElement} container
 * @constructor
 */
function AudioPlayer(container) {
  this.container_ = container;
  this.metadataProvider_ = new MetadataProvider();
  this.currentTrack_ = -1;

  function createChild(opt_className, opt_tag) {
    var child = container.ownerDocument.createElement(opt_tag || 'div');
    if (opt_className)
      child.className = opt_className;
    container.appendChild(child);
    return child;
  }

  this.playlist_ = createChild('playlist');

  createChild('title-button close').addEventListener(
      'click', function() { chrome.mediaPlayerPrivate.closeWindow() });

  createChild('title-button collapse').addEventListener(
      'click', this.onExpandCollapse_.bind(this));

  this.audioControls_ = new AudioControls(
      createChild(), this.advance_.bind(this));

  this.audioControls_.attachMedia(createChild('', 'audio'));
}

AudioPlayer.load = function() {
  var player = new AudioPlayer(document.querySelector('.audio-player'));
  function getPlaylist() {
    chrome.mediaPlayerPrivate.getPlaylist(true, player.load.bind(player));
  }
  getPlaylist();
  chrome.mediaPlayerPrivate.onPlaylistChanged.addListener(getPlaylist);
};

AudioPlayer.prototype.load = function(playlist) {
  this.stopTrack_();

  this.playlist_.textContent = '';

  this.trackInfo_ = [];

  for (var i = 0; i != playlist.items.length; i++) {
    var url = playlist.items[i].path;
    var trackInfo = new AudioPlayer.TrackInfo(
        this.playlist_, url, this.select_.bind(this, i));
    this.metadataProvider_.fetch(url, trackInfo.setMetadata.bind(trackInfo));
    this.trackInfo_.push(trackInfo);
  }

  if (this.trackInfo_.length == 1)
    this.container_.classList.add('single-track');
  else
    this.container_.classList.remove('single-track');

  this.currentTrack_ = 0;
  this.playTrack_();

  this.syncHeight_();
};

AudioPlayer.prototype.select_ = function(track) {
  if (this.currentTrack_ != track) {
    this.stopTrack_();
    this.currentTrack_ = track;
    this.playTrack_();
  }
};

AudioPlayer.prototype.stopTrack_ = function() {
  if (this.currentTrack_ != -1) {
    this.audioControls_.pause();
    this.trackInfo_[this.currentTrack_].select(false);
  }
};

AudioPlayer.prototype.playTrack_ = function() {
  var trackInfo = this.trackInfo_[this.currentTrack_];
  var media = this.audioControls_.getMedia();
  trackInfo.select(true);
  media.src = trackInfo.getUrl();
  media.load();
  this.audioControls_.play();
};

AudioPlayer.prototype.advance_ = function(forward) {
  var newTrack = this.currentTrack_ + (forward ? 1 : -1);
  if (newTrack < 0) newTrack = this.trackInfo_.length - 1;
  if (newTrack == this.trackInfo_.length) newTrack = 0;
  this.select_(newTrack);
};

AudioPlayer.prototype.onExpandCollapse_ = function() {
  this.container_.classList.toggle('collapsed');
  this.syncHeight_();
};

/* Keep the below constants in sync with the CSS. */
AudioPlayer.HEADER_HEIGHT = 30;
AudioPlayer.TRACK_HEIGHT = 58;
AudioPlayer.CONTROLS_HEIGHT = 35;

AudioPlayer.prototype.syncHeight_ = function() {
  var collapsed = this.container_.classList.contains('collapsed');
  var singleTrack = this.container_.classList.contains('single-track');
  var visibleTracks = Math.min(this.trackInfo_.length, collapsed ? 1 : 3);
  chrome.mediaPlayerPrivate.setWindowHeight(
    ((collapsed || singleTrack)? 0 : AudioPlayer.HEADER_HEIGHT) +
    visibleTracks * AudioPlayer.TRACK_HEIGHT +
    AudioPlayer.CONTROLS_HEIGHT);
};


/**
 * Create a TrackInfo object encapsulating the information about one track.
 *
 * @param {HTMLElement} container
 * @param {string} url
 * @param {function} onClick
 * @constructor
 */
AudioPlayer.TrackInfo = function(container, url, onClick) {
  this.url_ = url;

  var doc = container.ownerDocument;

  this.box_ = doc.createElement('div');
  this.box_.className = 'track';
  this.box_.addEventListener('click', onClick);
  container.appendChild(this.box_);

  this.art_ = doc.createElement('img');
  this.art_.className = 'art';
  this.box_.appendChild(this.art_);

  this.data_ = doc.createElement('div');
  this.data_.className = 'data';
  this.box_.appendChild(this.data_);

  this.title_ = doc.createElement('div');
  this.title_.className = 'data-title';
  this.data_.appendChild(this.title_);

  this.artist_ = doc.createElement('div');
  this.artist_.className = 'data-artist';
  this.data_.appendChild(this.artist_);
};

AudioPlayer.TrackInfo.prototype.getUrl = function() { return this.url_ };

AudioPlayer.TrackInfo.prototype.getDefaultArtwork = function() {
  return 'images/filetype_large_audio.png';
};

AudioPlayer.TrackInfo.prototype.getDefaultTitle = function() {
  var title = this.url_.split('/').pop();
  var dotIndex = title.lastIndexOf('.');
  if (dotIndex >= 0) title = title.substr(0, dotIndex);
  return title;
};

AudioPlayer.TrackInfo.prototype.getDefaultArtist = function() {
  return 'Unknown Artist';  // TODO(kaznacheev): i18n
};

AudioPlayer.TrackInfo.prototype.setMetadata = function(metadata) {
  this.art_.src = metadata.thumbnailURL || this.getDefaultArtwork();
  this.title_.textContent = metadata.title || this.getDefaultTitle();
  this.artist_.textContent = metadata.artist || this.getDefaultArtist();
};

AudioPlayer.TrackInfo.prototype.select = function(on) {
  if (on) {
    this.box_.classList.add('selected');

    // Scroll if necessary to make the selected item fully visible.
    var parent = this.box_.parentNode;
    parent.scrollTop = Math.max(
        this.box_.offsetTop + this.box_.offsetHeight - parent.offsetHeight,
        Math.min(this.box_.offsetTop, parent.scrollTop));
  } else {
    this.box_.classList.remove('selected');
  }
};
