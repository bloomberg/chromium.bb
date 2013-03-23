// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(function() {
  "use strict";

  var document = window.document;

  /**
   * Returns the URL of the ad image.
   */
  function getAdUrl(index) {
    var adPath = "ads/image315.png";
    var prefixIndex = location.href.lastIndexOf("/");
    return location.href.substr(0, prefixIndex) + "/" + adPath;
  }

  var messageSequenceNumber = 0;

  /**
   * Sends a message to the embedder app.
   */
  function sendAppMessage(source, message, publisherData, messageData) {
    source.postMessage({
      'sequenceNumber': messageSequenceNumber,
      'message': message,
      'publisherData': publisherData,
      'data': messageData,
    }, "*");

    messageSequenceNumber++;
  }

  /**
   * Called when the img for an ad is loaded into its iframe.
   */
  function onAdLoaded(source, publisherData) {
    // Resize containing iframe to size of image.
    var iframe = document.getElementById("ad-frame");
    var img = iframe.contentWindow.document.getElementsByTagName("IMG")[0];

    iframe.style.width = img.width + "px";
    iframe.style.height = img.height + "px";

    function getCombinedSize(style, propertyNames) {
      var result = 0;
      for(var i = 0; i < propertyNames.length; i++) {
        var value = style.getPropertyValue(propertyNames[i]);
        result += parseFloat(value.substr(0, value.indexOf("px")));
      }
      return result;
    }

    // Size of ad = size of document body + margins.
    var style = document.defaultView.getComputedStyle(document.body, '');
    var adHeight =
      getCombinedSize(style, ['height', 'margin-top', 'margin-bottom']);
    var adWidth =
      getCombinedSize(style, ['width', 'margin-left', 'margin-right']);

    var adRect = {
      height: adHeight + "px",
      width: adWidth + "px",
    };

    sendAppMessage(source, "ad-displayed", publisherData, {
      'adSize': {
        width: adRect.width,
        height: adRect.height
      }});

    img.onclick = function() {
      sendAppMessage(source, "ad-clicked", publisherData);
    };
  }

  /**
   * Displays the ad image in the ad iframe.
   */
  function displayAd(source, publisherData) {
    // Remove temporary message since we are about to display an add.
    var tempDiv = document.getElementById("temp-display");
    if (tempDiv) {
      tempDiv.parentNode.removeChild(tempDiv);
    }

    // Load image in iframe.
    var iframe = document.getElementById("ad-frame");
    if (iframe.contentWindow.document.body.firstChild == null) {
      var img = iframe.contentWindow.document.createElement("img");
      iframe.contentWindow.document.body.appendChild(img);
    }
    var img = iframe.contentWindow.document.body.firstChild;
    img.onload = function() { onAdLoaded(source, publisherData); };
    img.src = getAdUrl();
    iframe.style.visibility = "visible";
  }

  /**
   * Dispatches |appMessage| according to message value.
   */
  function processAppMessage(source, appMessage) {
    if (appMessage.message == "display-ad") {
      displayAd(source, appMessage.publisherData);
    }
    else if (appMessage.message == "display-first-ad") {
      displayAd(source, appMessage.publisherData);
    }
    else if (appMessage.message == "onloadcommit") {
      sendAppMessage(source, "onloadcommit-ack");
    }
  }

  /**
   * Handles "message" event.
   */
  function onPostMessage(eventMessage) {
    processAppMessage(eventMessage.source, eventMessage.data);
  }

  /**
   * Handles "DOMContentLoaded" event.
   */
  function onDocumentReady() {
    document.getElementById("url").textContent = window.location;
  }

  //
  // Register global event listeners.
  //
  window.addEventListener("message", onPostMessage, false);
  document.addEventListener('DOMContentLoaded', onDocumentReady, false);

})();
