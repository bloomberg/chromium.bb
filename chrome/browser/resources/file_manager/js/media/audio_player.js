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
  this.playlistGeneration_ = 0;

  this.container_.classList.add('collapsed');

  function createChild(opt_className, opt_tag) {
    var child = container.ownerDocument.createElement(opt_tag || 'div');
    if (opt_className)
      child.className = opt_className;
    container.appendChild(child);
    return child;
  }

  // We create two separate containers (for expanded and compact view) and keep
  // two sets of TrackInfo instances. We could fiddle with a single set instead
  // but it would make keeping the list scroll position very tricky.
  this.trackList_ = createChild('track-list');
  this.trackStack_ = createChild('track-stack');

  createChild('title-button close').addEventListener(
      'click', function() { chrome.mediaPlayerPrivate.closeWindow() });

  createChild('title-button collapse').addEventListener(
      'click', this.onExpandCollapse_.bind(this));

  this.audioControls_ = new AudioControls(
      createChild(), this.advance_.bind(this));

  this.audioControls_.attachMedia(createChild('', 'audio'));

  chrome.fileBrowserPrivate.getStrings(function(strings) {
    container.ownerDocument.title = strings['AUDIO_PLAYER_TITLE'];
  })
}

AudioPlayer.load = function() {
  document.ondragstart = function(e) { e.preventDefault() };
  document.oncontextmenu = function(e) { e.preventDefault(); };

  var player = new AudioPlayer(document.querySelector('.audio-player'));
  function getPlaylist() {
    chrome.mediaPlayerPrivate.getPlaylist(player.load.bind(player));
  }
  getPlaylist();
  chrome.mediaPlayerPrivate.onPlaylistChanged.addListener(getPlaylist);
};

AudioPlayer.prototype.load = function(playlist) {
  this.playlistGeneration_++;

  this.audioControls_.pause();

  this.currentTrack_ = -1;

  this.urls_ = playlist.items;

  if (this.urls_.length == 1)
    this.container_.classList.add('single-track');
  else
    this.container_.classList.remove('single-track');

  this.syncHeight_();

  this.trackList_.textContent = '';
  this.trackStack_.textContent = '';

  this.trackListItems_ = [];
  this.trackStackItems_ = [];

  for (var i = 0; i != this.urls_.length; i++) {
    var url = this.urls_[i];
    var onClick = this.select_.bind(this, i);
    this.trackListItems_.push(
        new AudioPlayer.TrackInfo(this.trackList_, url, onClick));
    this.trackStackItems_.push(
        new AudioPlayer.TrackInfo(this.trackStack_, url, onClick));
  }

  this.select_(playlist.position);

  // This class will be removed if at least one track has art.
  this.container_.classList.add('noart');

  // Load the selected track metadata first, then load the rest.
  this.loadMetadata_(playlist.position);
  for (i = 0; i != this.urls_.length; i++) {
    if (i != playlist.position)
      this.loadMetadata_(i);
  }
};

AudioPlayer.prototype.loadMetadata_ = function(track) {
  this.metadataProvider_.fetch(
      this.urls_[track],
      function(generation, metadata) {
        // Do nothing if another load happened since the metadata request.
        if (this.playlistGeneration_ != generation)
          return;

        if (metadata.thumbnailURL) {
          this.container_.classList.remove('noart');
        }
        this.trackListItems_[track].setMetadata(metadata);
        this.trackStackItems_[track].setMetadata(metadata);
      }.bind(this, this.playlistGeneration_));
};

AudioPlayer.prototype.select_ = function(newTrack) {
  if (this.currentTrack_ == newTrack) return;

  this.changeSelectionInList_(this.currentTrack_, newTrack);
  this.changeSelectionInStack_(this.currentTrack_, newTrack);

  this.currentTrack_ = newTrack;
  this.scrollToCurrent_(false);

  var media = this.audioControls_.getMedia();
  media.src = this.urls_[this.currentTrack_];
  media.load();
  this.audioControls_.play();
};

AudioPlayer.prototype.changeSelectionInList_ = function(oldTrack, newTrack) {
  this.trackListItems_[newTrack].getBox().classList.add('selected');

  if (oldTrack >= 0) {
    this.trackListItems_[oldTrack].getBox().classList.remove('selected');
  }
};

AudioPlayer.prototype.changeSelectionInStack_ = function(oldTrack, newTrack) {
  var newBox = this.trackStackItems_[newTrack].getBox();
  newBox.classList.add('selected');  // Put on top immediately.
  newBox.classList.add('visible');  // Start fading in.

  if (oldTrack >= 0) {
    var oldBox = this.trackStackItems_[oldTrack].getBox();
    oldBox.classList.remove('selected'); // Put under immediately.
    setTimeout(function () {
      if (!oldBox.classList.contains('selected')) {
        // This will start fading out which is not really necessary because
        // oldBox is already completely obscured by newBox.
        oldBox.classList.remove('visible');
      }
    }, 300);
  }
};

/**
 * Scrolls the current track into the viewport.
 *
 * @param {boolean} keepAtBottom If true, make the selected track the last
 *   of the visible (if possible). If false, perform minimal scrolling.
 */
AudioPlayer.prototype.scrollToCurrent_ = function(keepAtBottom) {
  var box = this.trackListItems_[this.currentTrack_].getBox();
  this.trackList_.scrollTop = Math.max(
      keepAtBottom ? 0 : Math.min(box.offsetTop, this.trackList_.scrollTop),
      box.offsetTop + box.offsetHeight - this.trackList_.clientHeight);
};

AudioPlayer.prototype.isCompact_ = function() {
  return this.container_.classList.contains('collapsed') ||
         this.container_.classList.contains('single-track');
};

AudioPlayer.prototype.advance_ = function(forward) {
  var newTrack = this.currentTrack_ + (forward ? 1 : -1);
  if (newTrack < 0) newTrack = this.urls_.length - 1;
  if (newTrack == this.urls_.length) newTrack = 0;
  this.select_(newTrack);
};

AudioPlayer.prototype.onExpandCollapse_ = function() {
  this.container_.classList.toggle('collapsed');
  this.syncHeight_();
  if (!this.isCompact_())
    this.scrollToCurrent_(true);
};

/* Keep the below constants in sync with the CSS. */
// TODO(kaznacheev): Set to 30 when the audio player is title-less.
AudioPlayer.HEADER_HEIGHT = 0;
AudioPlayer.TRACK_HEIGHT = 58;
AudioPlayer.CONTROLS_HEIGHT = 35;

AudioPlayer.prototype.syncHeight_ = function() {
  var expandedListHeight =
      Math.min(this.urls_.length, 3) * AudioPlayer.TRACK_HEIGHT;
  this.trackList_.style.height = expandedListHeight + 'px';

  chrome.mediaPlayerPrivate.setWindowHeight(
    (this.isCompact_() ?
        AudioPlayer.TRACK_HEIGHT :
        AudioPlayer.HEADER_HEIGHT + expandedListHeight) +
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

  this.art_ = doc.createElement('div');
  this.art_.className = 'art blank';
  this.box_.appendChild(this.art_);

  this.img_ = doc.createElement('img');
  this.art_.appendChild(this.img_);

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

AudioPlayer.TrackInfo.prototype.getBox = function() { return this.box_ };

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
  if (metadata.thumbnailURL) {
    this.art_.classList.remove('blank');
    this.img_.src = metadata.thumbnailURL;
  }
  this.title_.textContent = metadata.title || this.getDefaultTitle();
  this.artist_.textContent = metadata.artist || this.getDefaultArtist();
};