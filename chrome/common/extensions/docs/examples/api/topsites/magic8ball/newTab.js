// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function $(id) {
  return document.getElementById(id);
}

function thumbnailsGotten(data) {
  var eightBallWindow = $('mostVisitedThumb');
  var rand = Math.floor(Math.random() * data.length);
  eightBallWindow.style.backgroundImage =
      'url(chrome://thumb/' + data[rand].url + ')';
  eightBallWindow.href = data[rand].url;

  var span = eightBallWindow.querySelector('span');
  span.textContent = data[rand].title;
  span.style.backgroundImage = 'url(chrome://favicon/' + data[rand].url + ')';
}

window.onload = function() {
  chrome.experimental.topSites.get(thumbnailsGotten);
}
