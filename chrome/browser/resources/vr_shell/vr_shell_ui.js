// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var vrShellUi = (function() {
  'use strict';

  var scene = new ui.Scene();

  function initialize() {

    domLoaded();

    // Change the body background so that the transparency applies.
    window.setTimeout(function() {
      document.body.parentNode.style.backgroundColor = 'rgba(255,255,255,0)';
    }, 100);

    addControlButtons();
  }

  // Build a row of control buttons.
  function addControlButtons() {
    var buttons = [
        // Button text, UI action passed down to native.
        ['<', api.Action.HISTORY_BACK],
        ['>', api.Action.HISTORY_FORWARD],
        ['R', api.Action.RELOAD],
        ['-', api.Action.ZOOM_OUT],
        ['+', api.Action.ZOOM_IN]
    ];

    var buttonWidth = 0.3;
    var buttonHeight = 0.2;
    var buttonSpacing = 0.5;
    var buttonStartPosition = -buttonSpacing * (buttons.length / 2.0 - 0.5);

    for (var i = 0; i < buttons.length; i++) {
      var b = document.createElement('div');
      b.position = 'absolute';
      b.style.top = '384px';
      b.style.left = 50 * i + 'px';
      b.style.width = '50px';
      b.style.height = '50px';
      b.className = 'ui-button';
      b.textContent = buttons[i][0];

      // Add click behaviour.
      b.addEventListener('click', function(action, e) {
        api.doAction(action);
      }.bind(undefined, buttons[i][1]));

      document.body.appendChild(b);

      // Add a UI rectangle for the button.
      var el = new api.UiElement(50 * i, 384, 50, 50);
      el.setParentId(api.getContentElementId());
      el.setAnchoring(api.XAnchoring.XNONE, api.YAnchoring.YBOTTOM);
      el.setSize(buttonWidth, buttonHeight);
      el.setTranslation(buttonStartPosition + buttonSpacing * i, -0.3, 0.0);
      var buttonId = scene.addElement(el);

      // Add transitions when the mouse hovers over (and leaves) the button.
      b.addEventListener('mouseenter', function(buttonId, width, height, e) {
        var resize = new api.Animation(buttonId, 250);
        resize.setSize(width, height);
        scene.addAnimation(resize);
        scene.flush();
      }.bind(undefined, buttonId, buttonWidth * 1.5, buttonHeight * 1.5));
      b.addEventListener('mouseleave', function(buttonId, width, height) {
        var resize = new api.Animation(buttonId, 250);
        resize.setSize(width, height);
        scene.addAnimation(resize);
        scene.flush();
      }.bind(undefined, buttonId, buttonWidth, buttonHeight));
    }
    scene.flush();
  }

  function domLoaded() {
    api.domLoaded();
  }

  return {
    initialize: initialize,
  };
})();

document.addEventListener('DOMContentLoaded', vrShellUi.initialize);
