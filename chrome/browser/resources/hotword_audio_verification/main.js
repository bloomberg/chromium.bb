// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var appWindow = chrome.app.window.current();

document.addEventListener('DOMContentLoaded', function() {
  var flow = new Flow();
  flow.startFlow();

  var closeAppWindow = function(e) {
    var classes = e.target.classList;
    if (classes.contains('close') || classes.contains('finish-button')) {
      appWindow.close();
      e.preventDefault();
    }
  };

  // TODO(kcarattini): Cancel training in NaCl module for hotword-only and
  // speech-training close and cancel buttons.
  $('steps').addEventListener('click', closeAppWindow);

  $('hw-agree-button').addEventListener('click', function(e) {
    flow.advanceStep();
    e.preventDefault();
  });

  $('settings-link').addEventListener('click', function(e) {
    chrome.browser.openTab({'url': 'chrome://settings'}, function() {});
    e.preventDefault();
  });

  /**
   * Updates steps of the training UI.
   * @param {string} pagePrefix Prefix of the element ids for this page.
   * @param {Event} e The click event.
   */
  function doTraining(pagePrefix, e) {
    // TODO(kcarattini): Update this to respond to a hotword trigger once
    // speech training is implemented.
    var steps = $(pagePrefix + '-training').querySelectorAll('.train');
    var index = Array.prototype.indexOf.call(steps, e.currentTarget);
    if (!steps[index])
      return;

    // TODO(kcarattini): Localize this string.
    steps[index].querySelector('.text').textContent = 'Recorded';
    steps[index].classList.remove('listening');
    steps[index].classList.add('recorded');

    if (steps[index + 1]) {
      steps[index + 1].classList.remove('not-started');
      steps[index + 1].classList.add('listening');
    }
  }

  /**
   * Adds event listeners to the training UI.
   * @param {string} pagePrefix Prefix of the element ids for this page.
   */
  function addTrainingEventListeners(pagePrefix) {
    var steps = $(pagePrefix + '-training').querySelectorAll('.train');
    for (var i = 0; i < steps.length; ++i) {
      steps[i].addEventListener('click', function(e) {
        doTraining(pagePrefix, e);
        e.preventDefault();

        if (e.currentTarget != steps[steps.length - 1])
          return;

        // Update the 'Cancel' button.
        var buttonElem = $(pagePrefix + '-cancel-button');
        // TODO(kcarattini): Localize this string.
        buttonElem.textContent = 'Please wait ...';
        buttonElem.classList.add('grayed-out');
        buttonElem.classList.remove('finish-button');

        setTimeout(function() {
          if (chrome.hotwordPrivate.setAudioLoggingEnabled)
            chrome.hotwordPrivate.setAudioLoggingEnabled(true, function() {});

          if (chrome.hotwordPrivate.setHotwordAlwaysOnSearchEnabled) {
            chrome.hotwordPrivate.setHotwordAlwaysOnSearchEnabled(true,
                flow.advanceStep.bind(flow));
          }
        }, 2000);
      });
    }
  }

  addTrainingEventListeners('speech-training');
  addTrainingEventListeners('hotword-only');
});
