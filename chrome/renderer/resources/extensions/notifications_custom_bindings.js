// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the notifications API.
var binding = require('binding').Binding.create('notifications');

var sendRequest = require('sendRequest').sendRequest;
var imageUtil = require('imageUtil');
var lastError = require('lastError');

function image_data_setter(context, key) {
  var f = function(val) {
    this[key] = val;
  };
  return $Function.bind(f, context);
}

function replaceNotificationOptionURLs(notification_details, callback) {
  // A URL Spec is an object with the following keys:
  //  path: The resource to be downloaded.
  //  width: (optional) The maximum width of the image to be downloaded.
  //  height: (optional) The maximum height of the image to be downloaded.
  //  callback: A function to be called when the URL is complete. It
  //    should accept an ImageData object and set the appropriate
  //    field in the output of create.

  // TODO(dewittj): Try to remove hard-coding of image sizes.
  // |iconUrl| might be optional for notification updates.
  var url_specs = [];
  if (notification_details.iconUrl) {
    $Array.push(url_specs, {
      path: notification_details.iconUrl,
      width: 80,
      height: 80,
      callback: image_data_setter(notification_details, 'iconBitmap')
    });
  }

  // |imageUrl| is optional.
  if (notification_details.imageUrl) {
    $Array.push(url_specs, {
      path: notification_details.imageUrl,
      width: 360,
      height: 540,
      callback: image_data_setter(notification_details, 'imageBitmap')
    });
  }

  // Each button has an optional icon.
  var button_list = notification_details.buttons;
  if (button_list && typeof button_list.length === 'number') {
    var num_buttons = button_list.length;
    for (var i = 0; i < num_buttons; i++) {
      if (button_list[i].iconUrl) {
        $Array.push(url_specs, {
          path: button_list[i].iconUrl,
          width: 16,
          height: 16,
          callback: image_data_setter(button_list[i], 'iconBitmap')
        });
      }
    }
  }

  if (!url_specs.length) {
    callback(true);
    return;
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

function genHandle(name, failure_function) {
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
      lastError.run(name,
                    'Unable to download all specified images.',
                    null,
                    failure_function, [callback, id])
    });
  };
}

var handleCreate = genHandle('notifications.create',
                             function(callback, id) { callback(id); });
var handleUpdate = genHandle('notifications.update',
                             function(callback, id) { callback(false); });

var notificationsCustomHook = function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;
  apiFunctions.setHandleRequest('create', handleCreate);
  apiFunctions.setHandleRequest('update', handleUpdate);
};

binding.registerCustomHook(notificationsCustomHook);

exports.binding = binding.generate();
