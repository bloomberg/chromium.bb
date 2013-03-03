// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the notification API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;
var imageUtil = require('imageUtil');
var lastError = require('lastError');

function url_getter(context, key) {
  var f = function() {
    return this[key];
  };
  return f.bind(context);
}

function url_setter(context, key) {
  var f = function(val) {
    this[key] = val;
  };
  return f.bind(context);
}

function replaceNotificationOptionURLs(notification_details, callback) {
  // A URL Spec is an object with the following keys:
  //  path: The resource to be downloaded.
  //  callback: A function to be called when the URL is complete. It
  //    should accept an ImageData object and set the appropriate
  //    field in the output of create.

  // TODO(dewittj): Try to remove hard-coding.
  // |iconUrl| is required.
  var url_specs = [{
    path: notification_details.iconUrl,
    width: 80,
    height: 80,
    callback: url_setter(notification_details, 'iconUrl')
  }];

  // |secondIconUrl| is optional.
  if (notification_details.secondIconUrl) {
    url_specs.push({
      path: notification_details.secondIconUrl,
      width: 80,
      height: 80,
      callback: url_setter(notification_details, 'secondIconUrl')
    });
  }

  // |imageUrl| is optional.
  if (notification_details.imageUrl) {
    url_specs.push({
      path: notification_details.imageUrl,
      width: 300,
      height: 300,
      callback: url_setter(notification_details, 'imageUrl')
    });
  }

  // Each button has an optional icon.
  var button_list = notification_details.buttons;
  if (button_list && typeof button_list.length === 'number') {
    var num_buttons = button_list.length;
    for (var i = 0; i < num_buttons; i++) {
      if (button_list[i].iconUrl) {
        url_specs.push({
          path: button_list[i].iconUrl,
          width: 16,
          height: 16,
          callback: url_setter(button_list[i], 'iconUrl')
        });
      }
    }
  }

  var errors = 0;

  imageUtil.loadAllImages(url_specs, {
    onerror: function(index) {
      errors++;
    },
    oncomplete: function(imageData) {
      if (errors > 0) {
        callback(false);
        return;
      }
      for (var index = 0; index < url_specs.length; index++) {
        var url_spec = url_specs[index];
        url_spec.callback(imageData[index]);
      }
      callback(true);
    }
  });
}

function genHandle(failure_function) {
  return function(id, input_notification_details, callback) {
    // TODO(dewittj): Remove this hack. This is used as a way to deep
    // copy a complex JSON object.
    var notification_details = JSON.parse(
        JSON.stringify(input_notification_details));
    var that = this;
    replaceNotificationOptionURLs(notification_details, function(success) {
      if (success) {
        sendRequest(that.name,
            [id, notification_details, callback],
            that.definition.parameters);
        return;
      }
      lastError.set('Unable to download all specified images.');
      failure_function(callback, id);
    });
  };
}

var handleCreate = genHandle(function(callback, id) { callback(id); });
var handleUpdate = genHandle(function(callback, id) { callback(false); });

var experimentalNotificationCustomHook = function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;
  apiFunctions.setHandleRequest('create', handleCreate);
  apiFunctions.setHandleRequest('update', handleCreate);
};

chromeHidden.registerCustomHook('experimental.notification',
                                experimentalNotificationCustomHook);
