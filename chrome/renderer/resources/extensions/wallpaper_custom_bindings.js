// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var binding = require('binding').Binding.create('wallpaper');
var lastError = require('lastError');

var sendRequest = require('sendRequest').sendRequest;

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('setWallpaper',
                                function(details, callback) {
    var self = this;

    if ('wallpaperData' in details) {
      sendRequest(this.name,
                  [ details, callback ],
                  this.definition.parameters);
      return;
    } else if ('url' in details) {
      try {
        var xhr = new XMLHttpRequest();
        xhr.addEventListener('load', function(e) {
          if (this.status == 200) {
            if (xhr.response != null) {
              details.wallpaperData = xhr.response;
              sendRequest(self.name, [ details, callback ],
                          self.definition.parameters);
            }
          } else {
            lastError.run(
                'wallpaper' + self.name,
                'Failed to load wallpaper. Error code: ' + this.status,
                null,
                callback);
          }
        });
        xhr.addEventListener('error', function(e) {
          lastError.run('wallpaper' + self.name,
                        'Failed to load wallpaper. Error code: ' + this.status,
                        null,
                        callback);
        });
        xhr.open('GET', details.url, true);
        xhr.responseType = 'arraybuffer';
        xhr.send(null);
      } catch (e) {
        lastError.run('wallpaper' + this.name,
            'Exception in loading wallpaper',
            e.stack,
            callback);
      }
    }
  });
});

exports.binding = binding.generate();
