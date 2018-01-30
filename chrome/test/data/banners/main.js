// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const PromptAction = {
  CALL_PROMPT_DELAYED: 0,
  CALL_PROMPT_IN_HANDLER: 1,
  CANCEL_PROMPT: 2,
  STASH_EVENT: 3,
};

const LISTENER = "listener";
const ATTR = "attr";

// These blanks will get filled in when each event comes through.
let gotEventsFrom = ['_'.repeat(LISTENER.length), '_'.repeat(ATTR.length)];

let stashedEvent = null;

function initialize() {
  navigator.serviceWorker.register('service_worker.js');
}

function verifyEvents(eventName) {
  function setTitle() {
    window.document.title = 'Got ' + eventName + ': ' +
      gotEventsFrom.join(', ');
  }

  window.addEventListener(eventName, () => {
    gotEventsFrom[0] = LISTENER;
    setTitle();
  });
  window['on' + eventName] = () => {
    gotEventsFrom[1] = ATTR;
    setTitle();
  };
}

function callPrompt(event) {
  event.prompt();
}

function callStashedPrompt() {
  if (stashedEvent === null) {
      throw new Error('No event was previously stashed');
  }
  stashedEvent.prompt();
}

function addPromptListener(promptAction) {
  window.addEventListener('beforeinstallprompt', function(e) {
    e.preventDefault();

    switch (promptAction) {
      case PromptAction.CALL_PROMPT_DELAYED:
        setTimeout(callPrompt, 0, e);
        break;
      case PromptAction.CALL_PROMPT_IN_HANDLER:
        callPrompt(e);
        break;
      case PromptAction.CANCEL_PROMPT:
        // Navigate the window to trigger the banner cancellation.
        window.location.href = "/";
        break;
      case PromptAction.STASH_EVENT:
        stashedEvent = e;
        break;
    }
  });
}

function addManifestLinkTag() {
  var url = window.location.href;
  var manifestIndex = url.indexOf("?manifest=");
  var manifestUrl =
      (manifestIndex >= 0) ? url.slice(manifestIndex + 10) : 'manifest.json';
  var linkTag = document.createElement("link");
  linkTag.id = "manifest";
  linkTag.rel = "manifest";
  linkTag.href = manifestUrl;
  document.head.append(linkTag);
}

function changeManifestUrl(newManifestUrl) {
  var linkTag = document.getElementById("manifest");
  linkTag.href = newManifestUrl;
}
