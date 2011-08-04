// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The id of an extension we're using for install tests.
var extensionId = "enfkhcelefdadlmkffamgdlgplcionje";

// The id of an app we're using for install tests.
var appId = "iladmdjkfniedhfhcfoefgojhgaiaccc";

var assertEq = chrome.test.assertEq;
var assertNoLastError = chrome.test.assertNoLastError;
var callbackFail = chrome.test.callbackFail;
var callbackPass = chrome.test.callbackPass;
var listenOnce = chrome.test.listenOnce;
var runTests = chrome.test.runTests;
var succeed = chrome.test.succeed;

// Calls |callback| with true/false indicating whether an item with an id of
// extensionId is installed.
function checkInstalled(callback) {
  chrome.management.getAll(function(extensions) {
    callback(extensions.some(function(ext) {
      return ext.id == extensionId;
    }));
  });
}

var cachedIcon = null;
var img = null;

// This returns the base64-encoded content of the extension's image.
function getIconData(callback) {
  if (cachedIcon) {
    callback(cachedIcon);
  }
  var canvas = document.createElement("canvas");
  canvas.style.display = "none";
  canvas.width = 128;
  canvas.height = 128;
  img = new Image();
  img.onload = function() {
    console.log('img.onload called');
    var ctx = canvas.getContext("2d");
    ctx.drawImage(img, 0, 0);
    var tmp = canvas.toDataURL();
    // Strip the data url prefix to just get the base64-encoded bytes.
    cachedIcon = tmp.slice(tmp.search(",")+1);
    callback(cachedIcon);
  };
  img.src = "extension/icon.png";
}

var cachedManifest = null;

// This returns the string contents of the extension's manifest file.
function getManifest(alternativePath) {
  if (cachedManifest)
    return cachedManifest;

  // Do a synchronous XHR to get the manifest.
  var xhr = new XMLHttpRequest();
  xhr.open("GET",
           alternativePath ? alternativePath : "extension/manifest.json",
           false);
  xhr.send(null);
  return xhr.responseText;
}
