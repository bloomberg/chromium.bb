// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test clipboard extension api chrome.clipboard.onClipboardDataChanged event.

var testSuccessCount = 0;

function testSetImageDataClipboard(imageUrl, imageType, expectSucceed) {
  var oReq = new XMLHttpRequest();
  oReq.open('GET', imageUrl, true);
  oReq.responseType = 'arraybuffer';

  oReq.onload = function (oEvent) {
    var arrayBuffer = oReq.response;
    var binaryString = '';

    if (arrayBuffer) {
      chrome.clipboard.setImageData(arrayBuffer, imageType, function() {
        chrome.test.assertEq(expectSucceed, !chrome.runtime.lastError);
        chrome.test.succeed();
      });
    } else {
      chrome.test.fail('Failed to load the png image file');
    }
  };

  oReq.send(null);
}

function testSavePngImageToClipboard(baseUrl) {
  testSetImageDataClipboard(baseUrl + '/icon1.png', 'png', true);
}

function testSaveJpegImageToClipboard(baseUrl) {
  testSetImageDataClipboard(baseUrl + '/test.jpg', 'jpeg', true);
}

function testSaveBadImageData(baseUrl) {
  testSetImageDataClipboard(baseUrl + '/redirect_target.gif', 'jpeg', false);
}

function bindTest(test, param) {
  var result = test.bind(null, param);
  result.generatedName = test.name;
  return result;
}

chrome.test.getConfig(function(config) {
  var baseUrl = 'http://localhost:' + config.testServer.port + '/extensions';
  chrome.test.runTests([
    bindTest(testSavePngImageToClipboard, baseUrl),
    bindTest(testSaveJpegImageToClipboard, baseUrl),
    bindTest(testSaveBadImageData, baseUrl)
  ]);
})
