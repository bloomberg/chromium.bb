// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var vrShellUi = (function() {
  'use strict';

  var scene = new ui.Scene();
  var uiElements = [];

  class DomUiElement {
    constructor(domId) {
      var domElement = document.querySelector(domId);
      var style = window.getComputedStyle(domElement);

      // Pull copy rectangle from DOM element properties.
      var pixelX = domElement.offsetLeft;
      var pixelY = domElement.offsetTop;
      var pixelWidth = parseInt(style.getPropertyValue('width'));
      var pixelHeight = parseInt(style.getPropertyValue('height'));

      var element = new api.UiElement(pixelX, pixelY, pixelWidth, pixelHeight);
      element.setSize(pixelWidth / 1000, pixelHeight / 1000);

      this.uiElementId = scene.addElement(element);
      this.uiAnimationId = -1;
      this.domElement = domElement;
    }
  };

  class RoundButton extends DomUiElement {
    constructor(domId, callback) {
      super(domId);

      var button = this.domElement.querySelector('.button');
      button.addEventListener('mouseenter', this.onMouseEnter.bind(this));
      button.addEventListener('mouseleave', this.onMouseLeave.bind(this));
      button.addEventListener('click', callback);
    }

    configure(buttonOpacity, captionOpacity, distanceForward) {
      var button = this.domElement.querySelector('.button');
      var caption = this.domElement.querySelector('.caption');
      button.style.opacity = buttonOpacity;
      caption.style.opacity = captionOpacity;
      var anim = new api.Animation(this.uiElementId, 150);
      anim.setTranslation(0, 0, distanceForward);
      if (this.uiAnimationId >= 0) {
        scene.removeAnimation(this.uiAnimationId);
      }
      this.uiAnimationId = scene.addAnimation(anim);
      scene.flush();
    }

    onMouseEnter() {
      this.configure(1, 1, 0.015);
    }

    onMouseLeave() {
      this.configure(0.8, 0, 0);
    }
  };

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
        ['#back', function() { api.doAction(api.Action.HISTORY_BACK); }],
        ['#reload', function() { api.doAction(api.Action.RELOAD); }],
        ['#forward', function() { api.doAction(api.Action.HISTORY_FORWARD); }],
    ];

    var buttonSpacing = 0.3;
    var buttonStartPosition = -buttonSpacing * (buttons.length / 2.0 - 0.5);

    for (var i = 0; i < buttons.length; i++) {
      // Use an invisible parent to simplify Z-axis movement on hover.
      var position = new api.UiElement(0, 0, 0, 0);
      position.setParentId(api.getContentElementId());
      position.setVisible(false);
      position.setAnchoring(api.XAnchoring.XNONE, api.YAnchoring.YBOTTOM);
      position.setTranslation(
          buttonStartPosition + i * buttonSpacing, -0.3, 0.3);
      var id = scene.addElement(position);

      var domId = buttons[i][0];
      var callback = buttons[i][1];
      var element = new RoundButton(domId, callback);
      uiElements.push(element);

      var update = new api.UiElementUpdate();
      update.setParentId(id);
      update.setScale(2.2, 2.2, 1);
      scene.updateElement(element.uiElementId, update);
    }

    scene.flush();
  }

  function domLoaded() {
    api.domLoaded();
  }

  function command(dict) {
  }

  return {
    initialize: initialize,
    command: command,
  };
})();

document.addEventListener('DOMContentLoaded', vrShellUi.initialize);
