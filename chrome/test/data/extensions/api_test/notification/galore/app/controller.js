// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Galore = Galore || {};

Galore.controller = {

  BUTTON_IMAGE_SIZE: 64,
  NOTIFICATION_ICON_SIZE: 80,

  create: function() {
    var controller = Object.create(this);
    controller.counter = 0;
    controller.prefix = chrome.runtime.getURL('').slice(0, -1);
    controller.view = Galore.view.create(this.prepare_.bind(controller));
    controller.listen_('onDisplayed');
    controller.listen_('onError');
    controller.listen_('onClosed');
    controller.listen_('onClicked');
    controller.listen_('onButtonClicked');
    return controller;
  },

  /** @private */
  listen_: function(event) {
    var listener = this.event_.bind(this, event);
    chrome.experimental.notification[event].addListener(listener);
  },

  /** @private */
  prepare_: function() {
    Galore.NOTIFICATIONS.forEach(function (type) {
      type.notifications.forEach(function (options) {
        this.view.addNotificationButton(
            type.templateType,
            type.name,
            this.replace_(options.iconUrl, this.BUTTON_IMAGE_SIZE),
            this.notify_.bind(this, type.templateType, options));
      }, this);
    }, this);
  },

  /** @private */
  id_: function() {
    this.counter += 1;
    return String(this.counter);
  },

  /** @private */
  notify_: function(type, options) {
    var id = this.id_();
    var priority = this.view.getPriority();
    var expanded = this.expand_(options, type, priority);
    if (chrome.experimental.notification.create) {
      chrome.experimental.notification.create(id, expanded, function() {});
    } else {
      expanded.replaceId = id;
      delete expanded.buttonOneIconUrl;
      delete expanded.buttonOneTitle;
      delete expanded.buttonTwoIconUrl;
      delete expanded.buttonTwoTitle;
      chrome.experimental.notification.show(expanded, function() {});
    }
    this.event_('create', id, 'priority: ' + priority);
  },

  /** @private */
  expand_: function(options, type, priority) {
    var expanded = {templateType: type, priority: priority};
    Object.keys(options).forEach(function (key) {
      expanded[key] = this.replace_(options[key], this.NOTIFICATION_ICON_SIZE);
    }, this);
    return expanded;
  },

  /** @private */
  replace_: function(option, size) {
    var replaced;
    if (typeof option === 'string') {
      replaced = option.replace(/\$#/g, this.counter);
      replaced = replaced.replace(/\$@/g, this.prefix);
      replaced = replaced.replace(/\$%/g, size);
    } else if (Array.isArray(option)) {
      replaced = [];
      option.forEach(function(element) {
        replaced.push(this.replace_(element, size));
      }, this);
    } else {
      replaced = {};
      Object.keys(option).forEach(function (key) {
        replaced[key] = this.replace_(option[key], size);
      }, this);
    }
    return replaced;
  },

  /** @private */
  event_: function(event, id, var_args) {
    this.view.logEvent('Notification #' + id + ': ' + event + '(' +
                       Array.prototype.slice.call(arguments, 2).join(', ') +
                       ')');
  }

};
