// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Galore = Galore || {};

Galore.controller = {
  /** @constructor */
  create: function() {
    Galore.identifier = Galore.identifier ? (Galore.identifier + 1) : 1;
    var controller = Object.create(this);
    controller.identifier = Galore.identifier;
    controller.api = chrome;
    controller.counter = 0;
    return controller;
  },

  createWindow: function() {
    this.view = Galore.view.create(function() {
      var request = new XMLHttpRequest();
      request.open('GET', '/data/notifications.json', true);
      request.responseType = 'text';
      request.onload = this.onData_.bind(this, request);
      request.send();
    }.bind(this));
  },

  /** @private */
  onData_: function(request) {
    var data = JSON.parse(request.response);
    var path = data[0].api || 'notifications';
    this.api = chrome;
    path.split('.').forEach(function(key) {
      var before = this.api;
      this.api = this.api && this.api[key];
    }, this);
    if (this.api)
      this.onApi_(data);
    else
      this.view.logError('No API found - chrome.' + path + ' is undefined');
  },

  /** @private */
  onApi_: function(data) {
    var count = 0;
    var globals = data[0].globals || {};
    (data[0].events || []).forEach(function(event) {
      this.addListener_(event);
    }, this);
    data.forEach(function(section) {
      (section.notificationOptions || []).forEach(function(options) {
        var button = this.view.addNotificationButton(section.sectionName);
        Object.keys(section.globals || globals).forEach(function (key) {
          options[key] = options[key] || (section.globals || globals)[key];
        });
        ++count;
        this.fetchImages_(options, function() {
          var create = this.createNotification_.bind(this, section, options);
          var iconUrl = options.galoreIconUrl || options.iconUrl;
          delete options.galoreIconUrl;
          this.view.setNotificationButtonAction(button, create);
          this.view.setNotificationButtonIcon(button, iconUrl, options.title);
          if (--count == 0)
            this.view.showWindow();
        }.bind(this));
      }, this);
    }, this);
  },

  /** @private */
  onImages_: function(button, section, options) {
    var create = this.createNotification_.bind(this, section, options);
    var iconUrl = options.galoreIconUrl || options.iconUrl;
    delete options.galoreIconUrl;
    this.view.setNotificationButtonAction(button, create);
    this.view.setNotificationButtonIcon(button, iconUrl, options.title);
  },

  /** @private */
  fetchImages_: function(options, onFetched) {
    var count = 0;
    var replacements = {};
    this.mapStrings_(options, function(string) {
      if (string.indexOf("/images/") == 0 || string.indexOf("http:/") == 0) {
        ++count;
        this.fetchImage_(string, function(url) {
          replacements[string] = url;
          if (--count == 0) {
            this.mapStrings_(options, function(string) {
              return replacements[string] || string;
            });
            onFetched.call(this, options);
          }
        });
      }
    });
  },

  /** @private */
  fetchImage_: function(url, onFetched) {
    var request = new XMLHttpRequest();
    request.open('GET', url, true);
    request.responseType = 'blob';
    request.onload = function() {
      var url = window.URL.createObjectURL(request.response);
      onFetched.call(this, url);
    }.bind(this);
    request.send();
  },

  /** @private */
  createNotification_: function(section, options) {
    var id = this.getNextId_();
    var type = section.notificationType;
    var priority = this.view.settings.priority;
    var expanded = this.expandOptions_(options, id, type, priority);
    if (type == 'webkit')
      this.createWebKitNotification_(expanded);
    else
      this.createRichNotification_(expanded, id, type, priority);
  },

  /** @private */
  createWebKitNotification_: function(options) {
    var iconUrl = options.iconUrl;
    var title = options.title;
    var message = options.message;
    webkitNotifications.createNotification(iconUrl, title, message).show();
    this.handleEvent_('create', '?', 'title: "' + title + '"');
  },

  /** @private */
  createRichNotification_: function(options, id, type, priority) {
    this.api.create(id, options, function() {
      var argument1 = 'type: "' + type + '"';
      var argument2 = 'priority: ' + priority;
      var argument3 = 'title: "' + options.title + '"';
      this.handleEvent_('create', id, argument1, argument2, argument3);
    }.bind(this));
  },

  /** @private */
  getNextId_: function() {
    this.counter += 1;
    return String(this.counter);
  },

  /** @private */
  expandOptions_: function(options, id, type, priority) {
    var expanded = this.deepCopy_(options);
    return this.mapStrings_(expanded, function(string) {
      return this.expandOption_(string, id, type, priority);
    }, this);
  },
  /** @private */
  expandOption_: function(option, id, type, priority) {
    if (option == '$!') {
      option = priority;  // Avoids making priorities into strings.
    } else {
      option = option.replace(/\$#/g, id);
      option = option.replace(/\$\?/g, type);
      option = option.replace(/\$\!/g, priority);
    }
    return option;
  },

  /** @private */
  deepCopy_: function(value) {
    var copy = value;
    if (Array.isArray(value)) {
      copy = value.map(this.deepCopy_, this);
    } else if (value && typeof value === 'object') {
      copy = {}
      Object.keys(value).forEach(function (key) {
        copy[key] = this.deepCopy_(value[key]);
      }, this);
    }
    return copy;
  },

  /** @private */
  mapStrings_: function(value, map) {
    var mapped = value;
    if (typeof value === 'string') {
      mapped = map.call(this, value);
      mapped = (typeof mapped !== 'undefined') ? mapped : value;
    } else if (value && typeof value == 'object') {
      Object.keys(value).forEach(function (key) {
        mapped[key] = this.mapStrings_(value[key], map);
      }, this);
    }
    return mapped;
  },

  /** @private */
  addListener_: function(event) {
    var listener = this.handleEvent_.bind(this, event);
    if (this.api[event])
      this.api[event].addListener(listener);
    else
      console.log('Event ' + event + ' not defined.');
  },

  /** @private */
  handleEvent_: function(event, id, var_args) {
    this.view.logEvent('Notification #' + id + ': ' + event + '(' +
                       Array.prototype.slice.call(arguments, 2).join(', ') +
                       ')');
  }
};
