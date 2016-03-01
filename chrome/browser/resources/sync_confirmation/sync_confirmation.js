/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

cr.define('sync.confirmation', function() {
  'use strict';

  function onConfirm(e) {
    chrome.send('confirm');
  }

  function onUndo(e) {
    chrome.send('undo');
  }

  function onGoToSettings(e) {
    chrome.send('goToSettings');
  }

  function initialize() {
    $('confirmButton').addEventListener('click', onConfirm);
    $('undoButton').addEventListener('click', onUndo);
    $('settingsLink').addEventListener('click', onGoToSettings);
    $('profile-picture').addEventListener('load', onPictureLoaded);
    chrome.send('initialized');
  }

  function setUserImageURL(url) {
    $('profile-picture').src = url;
  }

  function onPictureLoaded(e) {
    $('picture-container').classList.add('loaded');
  }

  return {
    initialize: initialize,
    setUserImageURL: setUserImageURL
  };
});

document.addEventListener('DOMContentLoaded', sync.confirmation.initialize);
