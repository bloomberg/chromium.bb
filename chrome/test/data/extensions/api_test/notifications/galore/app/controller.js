// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Galore = Galore || {};

Galore.controller = {
  /** @constructor */
  create: function() {
    var controller = Object.create(this);
    controller.api = chrome;
    controller.counter = 0;
    return controller;
  },

  createWindow: function() {
    chrome.storage.sync.get('settings', this.onSettingsFetched_.bind(this));
  },

  /** @private */
  onSettingsFetched_: function(items) {
    var request = new XMLHttpRequest();
    var settings = items.settings || {};
    var source = settings.data || '/data/' + this.getDataVersion_();
    request.open('GET', source, true);
    request.responseType = 'text';
    request.onload = this.onDataFetched_.bind(this, settings, request);
    request.send();
  },

  /** @private */
  onDataFetched_: function(settings, request) {
    var count = 0;
    var data = JSON.parse(request.response);
    data.forEach(function(section) {
      (section.notificationOptions || []).forEach(function(options) {
        ++count;
        this.fetchImages_(options, function() {
          if (--count == 0)
            this.onImagesFetched_(settings, data);
        }.bind(this));
      }, this);
    }, this);
  },

  /** @private */
  onImagesFetched_: function(settings, data) {
    this.settings = settings;
    this.view = Galore.view.create(this.settings, function() {
      // Create buttons.
      data.forEach(function(section) {
        var defaults = section.globals || data[0].globals;
        var type = section.notificationType;
        (section.notificationOptions || []).forEach(function(options) {
          var defaulted = this.getDefaultedOptions_(options, defaults);
          var create = this.createNotification_.bind(this, type, defaulted);
          this.view.addNotificationButton(section.sectionName,
                                          defaulted.title,
                                          defaulted.iconUrl,
                                          create);
        }, this);
      }, this);
      // Set the API entry point and use it to set event listeners.
      this.api = this.getApi_(data);
      if (this.api)
        this.addListeners_(this.api, data[0].events);
      // Display the completed and ready window.
      this.view.showWindow();
    }.bind(this), this.onSettingsChange_.bind(this));
  },

  /** @private */
  fetchImages_: function(options, onFetched) {
    var count = 0;
    var replacements = {};
    this.mapStrings_(options, function(string) {
      if (string.indexOf("/images/") == 0 || string.search(/https?:\//) == 0) {
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
  onSettingsChange_: function(settings) {
    this.settings = settings;
    chrome.storage.sync.set({settings: this.settings});
  },

  /** @private */
  createNotification_: function(type, options) {
    var id = this.getNextId_();
    var priority = Number(this.settings.priority || 0);
    var expanded = this.getExpandedOptions_(options, id, type, priority);
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
  getDefaultedOptions_: function(options, defaults) {
    var defaulted = this.deepCopy_(options);
    Object.keys(defaults || {}).forEach(function (key) {
      defaulted[key] = options[key] || defaults[key];
    });
    return defaulted;
  },

  /** @private */
  getExpandedOptions_: function(options, id, type, priority) {
    var expanded = this.deepCopy_(options);
    return this.mapStrings_(expanded, function(string) {
      return this.getExpandedOption_(string, id, type, priority);
    }, this);
  },

  /** @private */
  getExpandedOption_: function(option, id, type, priority) {
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
  addListeners_: function(api, events) {
    (events || []).forEach(function(event) {
      var listener = this.handleEvent_.bind(this, event);
      if (api[event])
        api[event].addListener(listener);
      else
        console.log('Event ' + event + ' not defined.');
    }, this);
  },

  /** @private */
  handleEvent_: function(event, id, var_args) {
    this.view.logEvent('Notification #' + id + ': ' + event + '(' +
                       Array.prototype.slice.call(arguments, 2).join(', ') +
                       ')');
  },

  /** @private */
  getDataVersion_: function() {
    var version = navigator.appVersion.replace(/^.* Chrome\//, '');
    return (version > '27.0.1433.1') ? '27.0.1433.1.json' :
           (version > '27.0.1432.2') ? '27.0.1432.2.json' :
           '27.0.0.0.json';
  },

  /** @private */
  getApi_: function(data) {
    var path = data[0].api || 'notifications';
    var api = chrome;
    path.split('.').forEach(function(key) { api = api && api[key]; });
    if (!api)
      this.view.logError('No API found - chrome.' + path + ' is undefined');
    return api;
  }

};
