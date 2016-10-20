// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var vrShellUi = (function() {
  'use strict';

  var scene = new ui.Scene();
  var state;

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

  class Controls {
    constructor() {
      this.buttons = [];
      var descriptors = [
          ['#back', function() {
            api.doAction(api.Action.HISTORY_BACK);
          }],
          ['#reload', function() {
            api.doAction(api.Action.RELOAD);
          }],
          ['#forward', function() {
            api.doAction(api.Action.HISTORY_FORWARD);
          }],
      ];

      var spacing = 0.3;
      var startPosition = -spacing * (descriptors.length / 2.0 - 0.5);

      for (var i = 0; i < descriptors.length; i++) {
        // Use an invisible parent to simplify Z-axis movement on hover.
        var position = new api.UiElement(0, 0, 0, 0);
        position.setParentId(api.getContentElementId());
        position.setVisible(false);
        position.setAnchoring(api.XAnchoring.XNONE, api.YAnchoring.YBOTTOM);
        position.setTranslation(
            startPosition + i * spacing, -0.3, 0.3);
        var id = scene.addElement(position);

        var domId = descriptors[i][0];
        var callback = descriptors[i][1];
        var element = new RoundButton(domId, callback);
        this.buttons.push(element);

        var update = new api.UiElementUpdate();
        update.setParentId(id);
        update.setVisible(false);
        update.setScale(2.2, 2.2, 1);
        scene.updateElement(element.uiElementId, update);
      }
    }

    show(visible) {
      for (var i = 0; i < this.buttons.length; i++) {
        var update = new api.UiElementUpdate();
        update.setVisible(visible);
        scene.updateElement(this.buttons[i].uiElementId, update);
      }
    }
  };

  class UiState {
    constructor() {
      this.mode = api.Mode.UNKNOWN;
      this.controls = new Controls();
      scene.flush();
    }

    setMode(mode) {
      this.controls.show(mode == api.Mode.STANDARD);
      scene.flush();
    }
  };

  function initialize() {

    // Change the body background so that the transparency applies.
    window.setTimeout(function() {
      document.body.parentNode.style.backgroundColor = 'rgba(255,255,255,0)';
    }, 100);

    state = new UiState();

    api.domLoaded();
  }

  function command(dict) {
    if ('mode' in dict) {
      state.setMode(dict['mode']);
    }
  }

  return {
    initialize: initialize,
    command: command,
  };
})();

document.addEventListener('DOMContentLoaded', vrShellUi.initialize);
