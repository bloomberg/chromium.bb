// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(function() {
  "use strict";

  var document = window.document;

  // Arbitrary data.
  var publisherData = {
    id: "rpaquayAds",
    extras: {
      colorText: "Red",
      colorBg: "Default"
    },
  };

  /**
   * Displays message |statusText|, clearing it after 15 seconds.
   */
  var timeoutSequenceId = 0;
  function displayStatus(statusText) {
    timeoutSequenceId++;

    var div = document.getElementById("my-status");
    div.textContent = statusText;

    var sequenceId = timeoutSequenceId;
    setTimeout(function() {
      if (sequenceId === timeoutSequenceId) {
        div.textContent = "";
      }
    }, 15000);
  }

  /**
   * Displays size of ads when an ad has just been displayed.
   */
  function adDisplayed(source, appMessage) {
    var adview = document.getElementById("my-adview");
    adview.style.height = appMessage.data.adSize.height;

    displayStatus("Ad displayed( " + appMessage.sequenceNumber + "): " +
                  "height=" + appMessage.data.adSize.height);
  }

  /**
   * Displays publisher data information when an ad has just been clicked.
   */
  function adClicked(source, appMessage) {
    displayStatus("Ad clicked(" + appMmessage.sequenceNumber + "): " +
                  "publisher id=" + appMessage.publisherData.id);
  }

  /**
   * Dispatches |appMessage| according to message value.
   */
  function processAppMessage(source, appMessage) {
    if (appMessage.message == "ad-displayed") {
      adDisplayed(source, appMessage);
    }
    else if (appMessage.message == "ad-clicked") {
      adClicked(source, appMessage);
    }
  }

  /**
   * Handles "message" event.
   */
  function onPostMessage(event) {
    processAppMessage(event.source, event.data);
  }

  /**
   * Handles "DOMContentLoaded" event.
   */
  function onDocumentReady() {
    var button = document.getElementById('display-ad');
    var adview = document.getElementById("my-adview");

    // Enable "Display Ad" button when the adview content is loaded
    adview.addEventListener('loadcommit', function() {
      button.disabled = false;
      button.value = "Display Ad";
      button.addEventListener('click', function () {
        adview.contentWindow.postMessage({
          message: "display-ad",
          publisherData: publisherData
        }, "*");
      }, false);
    });
  }

  //
  // Register global event listeners.
  //
  window.addEventListener("message", onPostMessage, false);
  document.addEventListener('DOMContentLoaded', onDocumentReady, false);

})();
