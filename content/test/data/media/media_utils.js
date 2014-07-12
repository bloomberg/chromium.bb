// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var QueryString = function() {
  // Allows access to query parameters on the URL; e.g., given a URL like:
  //    http://<server>/my.html?test=123&bob=123
  // Parameters can then be accessed via QueryString.test or QueryString.bob.
  var params = {};
  // RegEx to split out values by &.
  var r = /([^&=]+)=?([^&]*)/g;
  // Lambda function for decoding extracted match values. Replaces '+' with
  // space so decodeURIComponent functions properly.
  function d(s) { return decodeURIComponent(s.replace(/\+/g, ' ')); }
  var match;
  while (match = r.exec(window.location.search.substring(1)))
    params[d(match[1])] = d(match[2]);
  return params;
}();

function failTest(msg) {
  var failMessage = msg;
  if (msg instanceof Event)
    failMessage = msg.target + '.' + msg.type;
  console.log("FAILED TEST: " + msg);
  setResultInTitle('FAILED');
}

var titleChanged = false;
function setResultInTitle(title) {
  // If document title is 'ENDED', then update it with new title to possibly
  // mark a test as failure.  Otherwise, keep the first title change in place.
  if (!titleChanged || document.title.toUpperCase() == 'ENDED')
    document.title = title.toUpperCase();
  console.log('Set document title to: ' + title + ', updated title: ' +
              document.title);
  titleChanged = true;
}

function installTitleEventHandler(element, event) {
  element.addEventListener(event, function(e) {
    setResultInTitle(event.toString());
  }, false);
}

function convertToArray(input) {
  if (Array.isArray(input))
    return input;
  return [input];
}
