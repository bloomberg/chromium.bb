// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Galore = Galore || {};

Galore.view = {
  /** @constructor */
  create: function(settings, onload, onSettingsChange) {
    var view = Object.create(this);
    view.actions = [];
    view.sections = {};
    view.settings = settings;
    view.onload = onload;
    view.onsettings = onSettingsChange;
    chrome.app.window.create('window.html', {
      id: 'window',
      frame: 'none',
      defaultWidth: 440, minWidth: 440, maxWidth: 440,
      defaultHeight: 640, minHeight: 640, maxHeight: 640,
      hidden: true
    }, function(appWindow) {
      view.window = appWindow;
      view.addListener_(appWindow.contentWindow, 'load', 'onLoad_');
    }.bind(this));
    return view;
  },

  addNotificationButton: function(sectionTitle,
                                  buttonTitle,
                                  iconUrl,
                                  onClick) {
    var button = this.getElement_('#templates .notification').cloneNode(true);
    var image = button.querySelector('img');
    image.src = iconUrl;
    image.alt = buttonTitle;
    button.name = buttonTitle;
    button.dataset.actionIndex = this.actions.push(onClick) - 1;
    this.addButtonListeners_(button);
    this.getSection_(sectionTitle).appendChild(button);
  },

  showWindow: function() {
    if (this.window)
      this.window.show();
  },

  logEvent: function(message) {
    var event = this.getElement_('#templates .event').cloneNode(true);
    event.textContent = message;
    this.getElement_('#events-scroll').appendChild(event).scrollIntoView();
  },

  logError: function(message) {
    var events = this.getElement_('#events-scroll');
    var error = this.getElement_('#templates .error').cloneNode(true);
    error.textContent = message;
    events.appendChild(error).scrollIntoView();
  },

  /** @private */
  onLoad_: function() {
    this.dataset = this.window.contentWindow.document.body.dataset;
    this.dataset.priority = this.settings.priority || '0';
    this.addListener_('body', 'mousedown', 'onBodyMouseDown_');
    this.addListener_('body', 'mouseup', 'onBodyMouseUp_');
    this.addListener_('#shadow', 'mousemove', 'onButtonMouseMove_');
    this.addButtonListeners_('button, #shadow');
    this.setButtonAction_('.priority', 'changePriority_');
    this.setButtonAction_('#shadow', 'toggleMenu_');
    this.setButtonAction_('#clear-events', 'clearEvents_');
    this.setButtonAction_('#show-menu', 'toggleMenu_');
    this.setButtonAction_('#close', 'close', this.window.contentWindow);
    if (this.onload)
      this.onload.call(this, this);
  },

  /**
   * Handling our own mouse events is fun! It also allows us to keep the cursor
   * appropriately indicating whether a button press or window drag is happening
   * or will happen on mousedown. As a bonus, it allows button to have a
   * highlight-as-you-drag behavior similar to menu items.
   *
   * @private */
  onButtonMouseDown_: function(event) {
    // Record the fact that a button in this button's group is active, which
    // allows onButtonMouseUp_ to do the right thing and CSS rules to correctly
    // set cursor types and button highlighting.
    var element = event.currentTarget;
    this.dataset.active = element.classList[0] || '';
    this.dragging = false;
  },

  /**
    * See the comment for onButtonMouseDown_() above.
    *
    * @private */
  onButtonMouseMove_: function(event) {
    // Buttons that support dragging add this as a listener to their mousemove
    // events so a flag can be set during dragging to prevent their actions from
    // being fired by the mouseup event that completes the dragging.
    this.dragging = true;
  },

  /**
    * See the comment for onButtonMouseDown_() above.
    *
    * @private */
  onButtonMouseOut_: function(event) {
    // Record the fact that the mouse is not over this button any more to allow
    // CSS rules to stop highlighting it.
    event.currentTarget.dataset.hover = 'false';
  },

  /**
    * See the comment for onButtonMouseDown_() above.
    *
    * @private */
  onButtonMouseOver_: function(event) {
    // Record the fact that the mouse is over this button to allow CSS rules to
    // highlight it if appropriate.
    event.currentTarget.dataset.hover = 'true';
  },

  /**
    * See the comment for onButtonMouseDown_() above.
    *
    * @private */
  onButtonMouseUp_: function(event) {
    // Send a button action if the button in which the mouseup happened is in
    // the same group as the one in which the mousedown happened. The regular
    // click handling which this replaces would send the action only if the
    // mouseup happened in the same button as the mousedown.
    var element = event.currentTarget;
    var group = (element.classList[0] || 'x');
    if (group == this.dataset.active && !this.dragging)
      this.actions[element.dataset.actionIndex].call(element, event);
  },

  /**
    * See the comment for onButtonMouseDown_() above.
    *
    * @private */
  onBodyMouseUp_: function(event) {
    // Record the fact that no button is active, which allows onButtonMouseUp_
    // to do the right thing and CSS rules to correctly set cursor types and
    // button highlighting.
    this.dataset.active = '';
  },

  /** @private */
  onBodyMouseDown_: function(event) {
    // Prevents the cursor from becoming a text cursor during drags.
    if (window.getComputedStyle(event.target).cursor != 'text')
      event.preventDefault();
  },

  /** @private */
  changePriority_: function(event) {
    this.settings.priority = event.currentTarget.dataset.priority || '0';
    this.dataset.priority = this.settings.priority;
    if (this.onsettings)
      this.onsettings.call(this, this.settings);
  },

  /** @private */
  toggleMenu_: function() {
    this.dataset.popup = String(this.dataset.popup != 'true');
  },

  /** @private */
  clearEvents_: function() {
    var events = this.getElement_('#events-scroll');
    while (events.lastChild)
      events.removeChild(events.lastChild);
    this.dataset.popup = 'false';
  },

  /** @private */
  getSection_: function(title) {
    this.sections[title] = (this.sections[title] || this.makeSection_(title));
    return this.sections[title];
  },

  /** @private */
  makeSection_: function(title) {
    var section = this.getElement_('#templates .section').cloneNode(true);
    section.querySelector('span').textContent = title;
    return this.getElement_('#notifications').appendChild(section);
  },

  /** @private */
  addButtonListeners_: function(buttons) {
    this.addListener_(buttons, 'mousedown', 'onButtonMouseDown_');
    this.addListener_(buttons, 'mouseout', 'onButtonMouseOut_');
    this.addListener_(buttons, 'mouseover', 'onButtonMouseOver_');
    this.addListener_(buttons, 'mouseup', 'onButtonMouseUp_');
  },

  /** @private */
  setButtonAction_: function(elements, action, target) {
    var index = this.actions.push(this.bind_(action, target)) - 1;
    this.getElements_(elements).forEach(function(element) {
      element.dataset.actionIndex = index;
    });
  },

  /** @private */
  addListener_: function(elements, event, action, target) {
    var listener = this.bind_(action, target);
    this.getElements_(elements).forEach(function(element) {
      element.addEventListener(event, listener);
    });
  },

  /** @private */
  bind_: function(action, target) {
    return (target || this)[action].bind(target || this);
  },

  /** @private */
  getElement_: function(element) {
    return this.getElements_(element)[0];
  },

  /** @private */
  getElements_: function(elements) {
    if (typeof elements === 'string')
      elements = this.window.contentWindow.document.querySelectorAll(elements);
    if (String(elements) === '[object NodeList]')
      elements = Array.prototype.slice.call(elements);
    return Array.isArray(elements) ? elements : [elements];
  }
};
