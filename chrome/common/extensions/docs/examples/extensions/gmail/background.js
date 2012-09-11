// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Identifier used to debug the possibility of multiple instances of the
// extension making requests on behalf of a single user.
var instanceId = 'gmc' + parseInt(Date.now() * Math.random(), 10);
var animationFrames = 36;
var animationSpeed = 10; // ms
var canvas;
var canvasContext;
var loggedInImage;
var pollIntervalMin = 5;  // 5 minutes
var pollIntervalMax = 60;  // 1 hour
var requestFailureCount = 0;  // used for exponential backoff
var requestTimeout = 1000 * 2;  // 2 seconds
var rotation = 0;
var unreadCount = -1;
var loadingAnimation = new LoadingAnimation();

// Legacy support for pre-event-pages.
var oldChromeVersion = !chrome.runtime;
var requestTimerId;

function getGmailUrl() {
  var url = "https://mail.google.com/";
  if (localStorage.customDomain)
    url += localStorage.customDomain + "/";
  else
    url += "mail/"
  return url;
}

function getFeedUrl() {
  // "zx" is a Gmail query parameter that is expected to contain a random
  // string and may be ignored/stripped.
  return getGmailUrl() + "feed/atom?zx=" + encodeURIComponent(instanceId);
}

function isGmailUrl(url) {
  // This is the Gmail we're looking for if:
  // - starts with the correct gmail url
  // - doesn't contain any other path chars
  var gmail = getGmailUrl();
  if (url.indexOf(gmail) != 0)
    return false;

  return url.length == gmail.length || url[gmail.length] == '?' ||
                       url[gmail.length] == '#';
}

// A "loading" animation displayed while we wait for the first response from
// Gmail. This animates the badge text with a dot that cycles from left to
// right.
function LoadingAnimation() {
  this.timerId_ = 0;
  this.maxCount_ = 8;  // Total number of states in animation
  this.current_ = 0;  // Current state
  this.maxDot_ = 4;  // Max number of dots in animation
}

LoadingAnimation.prototype.paintFrame = function() {
  var text = "";
  for (var i = 0; i < this.maxDot_; i++) {
    text += (i == this.current_) ? "." : " ";
  }
  if (this.current_ >= this.maxDot_)
    text += "";

  chrome.browserAction.setBadgeText({text:text});
  this.current_++;
  if (this.current_ == this.maxCount_)
    this.current_ = 0;
}

LoadingAnimation.prototype.start = function() {
  if (this.timerId_)
    return;

  var self = this;
  this.timerId_ = window.setInterval(function() {
    self.paintFrame();
  }, 100);
}

LoadingAnimation.prototype.stop = function() {
  if (!this.timerId_)
    return;

  window.clearInterval(this.timerId_);
  this.timerId_ = 0;
}

function init() {
  canvas = document.getElementById('canvas');
  loggedInImage = document.getElementById('logged_in');
  canvasContext = canvas.getContext('2d');
  updateIcon();
}

function updateIcon() {
  if (unreadCount == -1) {
    chrome.browserAction.setIcon({path:"gmail_not_logged_in.png"});
    chrome.browserAction.setBadgeBackgroundColor({color:[190, 190, 190, 230]});
    chrome.browserAction.setBadgeText({text:"?"});
  } else {
    chrome.browserAction.setIcon({path: "gmail_logged_in.png"});
    chrome.browserAction.setBadgeBackgroundColor({color:[208, 0, 24, 255]});
    chrome.browserAction.setBadgeText({
      text: unreadCount != "0" ? unreadCount : ""
    });
  }
}

function scheduleRequest() {
  var randomness = Math.random() * 2;
  var exponent = Math.pow(2, requestFailureCount);
  var multiplier = Math.max(randomness * exponent, 1);
  var delay = Math.min(multiplier * pollIntervalMin, pollIntervalMax);

  if (oldChromeVersion) {
    if (requestTimerId) {
      window.clearTimeout(requestTimerId);
    }
    requestTimerId = window.setTimeout(onAlarm, delay*60*1000);
  } else {
    chrome.alarms.create({'delayInMinutes': delay});
  }
}

// ajax stuff
function startRequest(params) {
  function doCallback() {
    if (params && params.scheduleRequest) scheduleRequest();
  }

  function stopLoadingAnimation() {
    if (params && params.showLoadingAnimation) loadingAnimation.stop();
  }

  if (params && params.showLoadingAnimation)
    loadingAnimation.start();

  getInboxCount(
    function(count) {
      stopLoadingAnimation();
      updateUnreadCount(count);
      doCallback();
    },
    function() {
      stopLoadingAnimation();
      unreadCount = -1;
      updateIcon();
      doCallback();
    }
  );
}

function getInboxCount(onSuccess, onError) {
  var xhr = new XMLHttpRequest();
  var abortTimerId = window.setTimeout(function() {
    xhr.abort();  // synchronously calls onreadystatechange
  }, requestTimeout);

  function handleSuccess(count) {
    requestFailureCount = 0;
    window.clearTimeout(abortTimerId);
    if (onSuccess)
      onSuccess(count);
  }

  var invokedErrorCallback = false;
  function handleError() {
    ++requestFailureCount;
    window.clearTimeout(abortTimerId);
    if (onError && !invokedErrorCallback)
      onError();
    invokedErrorCallback = true;
  }

  try {
    xhr.onreadystatechange = function(){
      if (xhr.readyState != 4)
        return;

      if (xhr.responseXML) {
        var xmlDoc = xhr.responseXML;
        var fullCountSet = xmlDoc.evaluate("/gmail:feed/gmail:fullcount",
            xmlDoc, gmailNSResolver, XPathResult.ANY_TYPE, null);
        var fullCountNode = fullCountSet.iterateNext();
        if (fullCountNode) {
          handleSuccess(fullCountNode.textContent);
          return;
        } else {
          console.error(chrome.i18n.getMessage("gmailcheck_node_error"));
        }
      }

      handleError();
    };

    xhr.onerror = function(error) {
      handleError();
    };

    xhr.open("GET", getFeedUrl(), true);
    xhr.send(null);
  } catch(e) {
    console.error(chrome.i18n.getMessage("gmailcheck_exception", e));
    handleError();
  }
}

function gmailNSResolver(prefix) {
  if(prefix == 'gmail') {
    return 'http://purl.org/atom/ns#';
  }
}

function updateUnreadCount(count) {
  var changed = unreadCount != count;
  unreadCount = count;
  updateIcon();
  if (changed)
    animateFlip();
}


function ease(x) {
  return (1-Math.sin(Math.PI/2+x*Math.PI))/2;
}

function animateFlip() {
  rotation += 1/animationFrames;
  drawIconAtRotation();

  if (rotation <= 1) {
    setTimeout(animateFlip, animationSpeed);
  } else {
    rotation = 0;
    drawIconAtRotation();
    chrome.browserAction.setBadgeText({
      text: unreadCount != "0" ? unreadCount : ""
    });
    chrome.browserAction.setBadgeBackgroundColor({color:[208, 0, 24, 255]});
  }
}

function drawIconAtRotation() {
  canvasContext.save();
  canvasContext.clearRect(0, 0, canvas.width, canvas.height);
  canvasContext.translate(
      Math.ceil(canvas.width/2),
      Math.ceil(canvas.height/2));
  canvasContext.rotate(2*Math.PI*ease(rotation));
  canvasContext.drawImage(loggedInImage,
      -Math.ceil(canvas.width/2),
      -Math.ceil(canvas.height/2));
  canvasContext.restore();

  chrome.browserAction.setIcon({imageData:canvasContext.getImageData(0, 0,
      canvas.width,canvas.height)});
}

function goToInbox() {
  chrome.tabs.getAllInWindow(undefined, function(tabs) {
    for (var i = 0, tab; tab = tabs[i]; i++) {
      if (tab.url && isGmailUrl(tab.url)) {
        chrome.tabs.update(tab.id, {selected: true});
        startRequest({scheduleRequest:false, showLoadingAnimation:false});
        return;
      }
    }
    chrome.tabs.create({url: getGmailUrl()});
  });
}

document.addEventListener('DOMContentLoaded', init);

function onInit() {
  startRequest({scheduleRequest:true, showLoadingAnimation:true});
}

if (oldChromeVersion) {
  onInit();
} else {
  chrome.runtime.onInstalled.addListener(onInit);
}

function onAlarm() {
  startRequest({scheduleRequest:true, showLoadingAnimation:false});
}
if (!oldChromeVersion)
  chrome.alarms.onAlarm.addListener(onAlarm);

var filters = {
  // TODO(aa): Cannot use urlPrefix because all the url fields lack the protocol
  // part. See crbug.com/140238.
  url: [{urlContains: getGmailUrl().replace(/^https?\:\/\//, '')}]
};

chrome.webNavigation.onDOMContentLoaded.addListener(function(changeInfo) {
  if (changeInfo.url && isGmailUrl(changeInfo.url)) {
    startRequest({scheduleRequest:false, showLoadingAnimation:false});
  }
}, filters);

chrome.browserAction.onClicked.addListener(goToInbox);
