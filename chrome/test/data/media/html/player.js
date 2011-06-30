// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Javascript that is needed for HTML files with the HTML5 media player.
// It does the following:
// * Parses query strings and sets the HTML tag.
// * Installs event handlers to change the HTML title.


function InstallEventHandler(event, action) {
  var player = document.getElementById('player');
  player.addEventListener(event, function(e) {
    eval(action);
  }, false);
}

var qs = new Array();

function defined(item) {
  return typeof item != 'undefined';
}

function queryString(key) {
  if (!defined(qs[key])) {
    var reQS = new RegExp('[?&]' + key + '=([^&$]*)', 'i');
    var offset = location.search.search(reQS);
    qs[key] = (offset >= 0)? RegExp.$1 : null;
  }
  return qs[key];
}

var media_url = queryString('media');
var ok = true;

if (!queryString('media')) {
  document.title = 'FAIL';
  ok = false;
}

if (defined(queryString('t'))) {
  // Append another parameter "t=" in url that disables media cache.
  media_url += '?t=' + (new Date()).getTime();
}

var tag = queryString('tag');

if (!queryString('tag')) {
  // Default tag is video tag.
  tag = 'video';
}

if (tag != 'audio' && tag != 'video') {
  document.title = 'FAIL';
  ok = false;
}

function translateCommand(command, arg) {
  // Translate command in 'actions' query string into corresponding JavaScript
  // code.
  if (command == 'seek') {
    return 'player.currentTime=' + arg + ';';
  } else if (command == 'ratechange') {
    return 'player.playbackRate=' + arg + ';';
  } else if (command == 'play' || command == 'pause') {
    return 'player.' + command + '();';
  } else {
    return 'ERROR - ' + command + ' is not a valid command.'
  }
}

var container = document.getElementById('player_container');
container.innerHTML = '<div id="main" width="0%" height="0%" ' +
                      'style="display: inline-block;"></div>' +
                      '<div id="extra"></div>';
// Create new player.
var newElement = document.createElement(tag);
newElement.setAttribute('id', 'player');
newElement.setAttribute('src', queryString('media'));
// Hide the video at the beginning for jerky test, in which
// we have to set predefined image before playing video.
if (queryString('jerky')) {
  newElement.setAttribute('style', 'opacity: 0;');
}
var main = document.getElementById('main');
main.appendChild(newElement);

// Install event handlers.
var player = document.getElementById('player');
InstallEventHandler('error',
                    'document.title = "ERROR = " + player.error.code');
InstallEventHandler('playing', 'document.title = "PLAYING"');
InstallEventHandler('ended', 'document.title = "END"');

if (queryString('num_extra')) {
  // Process query string for extra players.
  // Exra players use the exact same media file as the main player.
  for (var i = 0; i < queryString('num_extra'); i++) {
    var extra = document.getElementById('extra');
    var extraElement = document.createElement(tag);
    extraElement.setAttribute('id', 'player' + i);
    extraElement.setAttribute('src', media_url);
    extraElement.setAttribute('autoplay', 'true');
    extra.appendChild(extraElement);
  }
}

if (queryString('track')) {
  // Process query string for track (caption).
  // Set the track file name.
  // TODO(imasaki@chromium.org): add query parameters hardcoded here.
  var track_file = queryString('track');
  var trackElement = document.createElement('track');
  trackElement.setAttribute('id', 'track');
  trackElement.setAttribute('kind', 'captions');
  trackElement.setAttribute('src', track_file);
  trackElement.setAttribute('srclang', 'en');
  trackElement.setAttribute('label', 'English');
  trackElement.setAttribute('default', 'true');
  player.appendChild(trackElement);
}

if (queryString('actions')) {
  // Action query string is a list of actions. An action consists of a
  // time, action, action_argument triple (delimited by '|').
  // For example, '1000|seek|4000' means 'At second 1, seek to second 4.'
  // Or '1000|pause|0|2000|play|0' means 'At second 1, pause the video.
  // At second 2, play the video.'
  var original_actions = queryString('actions').split('|');
  if ((original_actions.length % 3) != 0) {
    // The action list is a list of triples. Otherwise, it fails.
    document.title = 'FAIL Action length=' + original_actions.length +
                     ' is not multiple of 3';
    ok = false;
  }
  for (var i = 0; i < original_actions.length / 3; i++) {
    setTimeout(translateCommand(original_actions[3 * i + 1],
                                original_actions[3 * i + 2]),
               parseInt(original_actions[3 * i]));
  }
}

// Used for playing the video in the media_jerky.py test.
function playMedia() {
  var player = document.getElementById('player');
  player.style.opacity = '1';
  player.play();
}

// Called in the body onload event in media_jerky.html.
function setPattern() {
  var main = document.getElementById('main');
  main.style.backgroundColor = '#50dead';
}

