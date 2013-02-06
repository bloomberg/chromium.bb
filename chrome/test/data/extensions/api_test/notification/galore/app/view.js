// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Galore = Galore || {};

Galore.view = {
  create: function(onLoad) {
    var view = Object.create(this);
    chrome.app.window.create('window.html', {
      id: 'window',
      frame: 'none',
      defaultWidth: 512, minWidth: 512, maxWidth: 512,
      defaultHeight: 736, minHeight: 736, maxHeight: 736,
    }, function(appWindow) {
      view.sections = {}
      view.window = appWindow.contentWindow;
      view.window.onload = this.loaded_.bind(view, onLoad);
    }.bind(this));
    return view;
  },

  addNotificationButton: function(sectionId, sectionTitle, imageUrl, onClick) {
    var section = this.section_(sectionId, sectionTitle);
    var button = this.button_(section, onClick);
    this.fetch_(imageUrl, button.querySelector('img'));
  },

  getPriority: function() {
    var inputs = this.elements_('#priority input');
    var checked = Array.prototype.filter.call(inputs, function(input) {
      return input.checked;
    });
    return (checked && checked.length) ? Number(checked[0].value) : 0;
  },

  logEvent: function(message) {
    var event = this.element_('#templates .event').cloneNode(true);
    event.textContent = message;
    this.element_('#events-scroll').appendChild(event).scrollIntoView();
  },

  /** @private */
  loaded_: function(onLoad) {
    this.element_('#close').onclick = this.window.close.bind(this.window);
    if (onLoad)
      onLoad.call(this);
  },

  /** @private */
  fetch_: function(url, image) {
    var request = new XMLHttpRequest();
    request.open('GET', url, true);
    request.responseType = 'blob';
    request.onload = this.fetched_.bind(this, request, image);
    request.send();
  },

  /** @private */
  fetched_: function(request, image) {
    image.src = window.URL.createObjectURL(request.response);
  },

  /** @private */
  section_: function(id, title) {
    if (!this.sections[id]) {
      this.sections[id] = this.element_('#templates .section').cloneNode(true);
      this.sections[id].querySelector('span').textContent = title;
      this.element_('#sections').appendChild(this.sections[id]);
    }
    return this.sections[id];
  },

  /** @private */
  button_: function(section, onClick) {
    var button = this.element_('#templates button');
    button = button.cloneNode(true);
    button.onclick = onClick;
    section.appendChild(button);
    return button;
  },

  /** @private */
  element_: function(selector) {
    return this.window.document.querySelector(selector)
  },

  /** @private */
  elements_: function(selector) {
    return this.window.document.querySelectorAll(selector)
  }
};
