// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function $(id) {
  return document.getElementById(id);
}

function getUrlSearchParams(search) {
  var params = {};

  if (search) {
    // Strips leading '?'
    search = search.substring(1);
    var pairs = search.split('&');
    for (var i = 0; i < pairs.length; ++i) {
      var pair = pairs[i].split('=');
      if (pair.length == 2) {
        params[pair[0]] = decodeURIComponent(pair[1]);
      } else {
        params[pair] = true;
      }
    }
  }

  return params;
}

function onPageLoad() {
  $('read_btn').addEventListener('click', onRead);
  $('write_btn').addEventListener('click', onWrite);
  $('truncate_btn').addEventListener('click', onTruncate);

  var listener = $('listener')
  listener.addEventListener('load', moduleDidLoad, true);
  listener.addEventListener('message', handleMessage, true);
  var params = getUrlSearchParams(location.search);
  $('filename').innerText = params['file'];
  $('content').innerText = 'Loading plugin...';
}

function moduleDidLoad(load_event) {
  $('content').innerText = 'Plugin loaded!';
  var params = getUrlSearchParams(location.search);
  onRead(params['file']);
}

function handleMessage(message_event) {
  console.log('Got message:');
  console.log(message_event);
  $('content').textContent = message_event.data;
}

function onRead(filePath) {
  $('file_reader').postMessage("readFile:" + filePath);
}

function onTruncate() {
  chrome.fileBrowserHandler.selectFile({ suggestedName: ''},
    function(result) {
      $('file_reader').postMessage("truncateFile:" + result.entry.fullPath);
    });
}

function onWrite() {
  chrome.fileBrowserHandler.selectFile({ suggestedName: ''},
    function(result) {
      $('file_reader').postMessage(
        "writeFile:" + result.entry.fullPath + ":" + $('result').textContent);
    });
}

window.addEventListener('load', onPageLoad, true);

