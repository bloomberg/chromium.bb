// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function $(criterion) {
  return document.querySelector(criterion);
}

var cyclerUI = new (function () {
  this.urlList = [];
  this.cacheDir = "";

  this.captureTab = $("#capture-tab");
  this.captureTabLabel = $("#capture-tab-label");
  this.captureButton = $("#capture-test");
  this.captureErrorDiv = $("#capture-errors-display");
  this.captureErrorList = $("#capture-errors");

  this.playbackTab = $("#playback-tab");
  this.playbackTabLabel = $("#playback-tab-label");
  this.playbackURLs = $("#playback-urls");
  this.playbackCache = $("#playback-cache-dir");
  this.playbackButton = $("#playback-test");
  this.playbackErrorDiv = $("#playback-errors-display");
  this.playbackErrorList = $("#playback-errors");

  this.playbackURLs.innerText = this.playbackCache.innerText = noTestMessage;

  this.enableTab = function(tabLabel, tab) {
    var tabList = document.querySelectorAll(".tab");
    var tabLabelList = document.querySelectorAll(".tab-label");

    for (var i = 0; i < tabList.length; i++)
      if (tabList[i] == tab)
        tabList[i].style.visibility = "visible";
      else
        tabList[i].style.visibility = "hidden";

    for (var i = 0; i < tabLabelList.length; i++)
      if (tabLabelList[i] == tabLabel) {
         tabLabelList[i].classList.add("enabled-tab-label");
         tabLabelList[i].classList.remove("disabled-tab-label");
      } else {
         tabLabelList[i].classList.remove("enabled-tab-label");
         tabLabelList[i].classList.add("disabled-tab-label");
      }
  }

  this.chooseCapture = function() {
    this.enableTab(this.captureTabLabel, this.captureTab);
  }

  this.chooseReplay = function() {
    this.enableTab(this.playbackTabLabel, this.playbackTab);
  }

  this.captureTest = function() {
    var errorList = $("#capture-errors");
    var errors = [];

    this.cacheDir = $("#capture-cache-dir").value;
    this.urlList = $("#capture-urls").value.split("\n");

    if (errors.length > 0) {
      this.captureErrorList.innerText = errors.join("\n");
      this.captureErrorDiv.className = "error-list-show";
    }
    else {
      this.captureErrorDiv.className = "error-list-hide";
      this.captureButton.disabled = true;
      chrome.experimental.record.captureURLs(this.urlList, this.cacheDir,
          this.onCaptureDone.bind(this));
    }
  }

  this.onCaptureDone = function(errors) {

    this.captureButton.disabled = false;
    if (errors.length > 0) {
      this.captureErrorList.innerText = errors.join("\n");
      this.captureErrorDiv.className = "error-list-show";
      this.playbackButton.disabled = true;
      this.playbackCache.innerText = this.playbackURLs.innerText =
          noTestMessage;
    }
    else {
      this.captureErrorDiv.className = "error-list-hide";
      this.playbackButton.disabled = false;
      this.playbackURLs.innerText = this.urlList.join("\n");
      this.playbackCache.innerText = this.cacheDir;
    }
  }

  this.playbackTest = function() {
    var extensionPath = $("#extension-dir").value;
    var repeatCount = parseInt($('#repeat-count').value);
    var errors = [];

    // Check local errors
    if (isNaN(repeatCount))
      errors.push("Enter a number for repeat count");
    else if (repeatCount < 1 || repeatCount > 100)
      errors.push("Repeat count must be between 1 and 100");

    if (errors.length > 0) {
      this.playbackErrorList.innerText = errors.join("\n");
      this.playbackErrorDiv.className = "error-list-show";
    } else {
      this.playbackErrorDiv.className = "error-list-hide";
      this.playbackButton.disabled = true;
      chrome.experimental.record.playbackURLs(
          this.urlList,
          this.cacheDir,
          repeatCount,
          {"extensionPath": extensionPath},
          this.onReplayDone.bind(this));
    }
  }

  this.onReplayDone = function(result) {
    var playbackResult = $("#playback-result");

    this.playbackButton.disabled = false;

    if (result.errors.length > 0) {
      this.playbackErrorList.innerText = result.errors.join("<br>");
      this.playbackErrorDiv.className = "error-list-show";
    }
    else {
      this.playbackErrorDiv.className = "error-list-hide";
      playbackResult.innerText = "Test took " + result.runTime + "mS :\n" +
        result.stats;
    }
  }

  this.captureButton.addEventListener("click", this.captureTest.bind(this));
  this.playbackButton.addEventListener("click", this.playbackTest.bind(this));
  this.captureTabLabel.addEventListener("click", this.chooseCapture.bind(this));
  this.playbackTabLabel.addEventListener("click", this.chooseReplay.bind(this));
  this.enableTab(this.captureTabLabel, this.captureTab);
})();

