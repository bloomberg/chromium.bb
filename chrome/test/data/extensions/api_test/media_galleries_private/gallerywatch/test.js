// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var galleries;
var invalidGalleryId = 11000;

// chrome.mediaGalleries.getMediaFileSystems callback.
var mediaFileSystemsListCallback = function (results) {
  galleries = results;
  chrome.test.sendMessage('get_media_file_systems_callback_ok');
};

// Gallery changed event handler.
var onGalleryChangedCallback = function (details) {
  chrome.test.sendMessage('gallery_changed_event_received');
};

// Add watch request callback.
var onAddWatchRequestCallback = function (details) {
  if (!details || !details.success)
    chrome.test.sendMessage('add_watch_request_failed');
  else
    chrome.test.sendMessage('add_watch_request_succeeded');
};

// Helpers to add and remove event listeners.
function addGalleryChangedListener() {
  chrome.mediaGalleriesPrivate.onGalleryChanged.addListener(
      onGalleryChangedCallback);
  chrome.test.sendMessage('add_gallery_changed_listener_ok');
};

function setupWatchOnValidGalleries() {
  for (var i = 0; i < galleries.length; ++i) {
    var info = JSON.parse(galleries[i].name);
    chrome.mediaGalleriesPrivate.addGalleryWatch(info.galleryId,
                                                 onAddWatchRequestCallback);
  }
  chrome.test.sendMessage('add_gallery_watch_ok');
};

function setupWatchOnInvalidGallery() {
  chrome.mediaGalleriesPrivate.addGalleryWatch(invalidGalleryId,
                                               onAddWatchRequestCallback);
  chrome.test.sendMessage('add_gallery_watch_ok');
}

function getMediaFileSystems() {
  chrome.mediaGalleries.getMediaFileSystems(mediaFileSystemsListCallback);
  chrome.test.sendMessage('get_media_file_systems_ok');
};

function removeGalleryWatch() {
  for (var i = 0; i < galleries.length; ++i) {
    var info = JSON.parse(galleries[i].name);
    chrome.mediaGalleriesPrivate.removeGalleryWatch(info.galleryId);
  }
  chrome.test.sendMessage('remove_gallery_watch_ok');
};

function removeGalleryChangedListener() {
  chrome.mediaGalleriesPrivate.onGalleryChanged.removeListener(
      onGalleryChangedCallback);
  chrome.test.sendMessage('remove_gallery_changed_listener_ok');
};
