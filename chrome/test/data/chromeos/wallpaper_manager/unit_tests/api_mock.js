// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var TestConstants = {
  wallpaperURL: 'https://test.com/test.jpg',
  // A dummy string which is used to mock an image.
  image: '*#*@#&'
};

// Mock a few chrome apis.
var chrome = {
  storage: {
    local: {
      get: function(key, callback) {
        var items = {};
        switch (key) {
          case Constants.AccessLocalWallpaperInfoKey:
            items[Constants.AccessLocalWallpaperInfoKey] = {
              'url': 'dummy',
              'layout': 'dummy',
              'source': 'dummy'
            };
        }
        callback(items);
      },
      set: function(items, callback) {
      }
    },
    sync: {
      get: function(key, callback) {
      },
      set: function(items, callback) {
      }
    },
    onChanged: {
      addListener: function(listener) {
        this.dispatch = listener;
      }
    }
  },
  syncFileSystem: {
    requestFileSystem: function(fs) {
    },
    onFileStatusChanged: {
      addListener: function(listener) {
        this.dispatch = listener;
      }
    }
  },
  app: {
    runtime: {
      onLaunched: {
        addListener: function(listener) {
        }
      }
    }
  },
  alarms: {
    onAlarm: {
      addListener: function(listener) {
      }
    }
  },
  wallpaperPrivate: {
    getStrings: function(callback) {
      callback({isExperimental: false});
    }
  }
};

(function (exports) {
  var originalXMLHttpRequest = window.XMLHttpRequest;

  // Mock XMLHttpRequest. It dispatches a 'load' event immediately with status
  // equals to 200 in function |send|.
  function MockXMLHttpRequest() {
  }

  MockXMLHttpRequest.prototype = {
    responseType: null,
    url: null,

    send: function(data) {
      this.status = 200;
      this.dispatchEvent('load');
    },

    addEventListener: function(type, listener) {
      this.eventListeners = this.eventListeners || {};
      this.eventListeners[type] = this.eventListeners[type] || [];
      this.eventListeners[type].push(listener);
    },

    removeEventListener: function (type, listener) {
      var listeners = this.eventListeners && this.eventListeners[type] || [];

      for (var i = 0; i < listeners.length; ++i) {
        if (listeners[i] == listener) {
          return listeners.splice(i, 1);
        }
      }
    },

    dispatchEvent: function(type) {
      var listeners = this.eventListeners && this.eventListeners[type] || [];

      if (/test.jpg$/g.test(this.url)) {
        this.response = TestConstants.image;
      } else {
        this.response = '';
      }

      for (var i = 0; i < listeners.length; ++i)
        listeners[i].call(this, new Event(type));
    },

    open: function(method, url, async) {
      this.url = url;
    }
  };

  function installMockXMLHttpRequest() {
    window['XMLHttpRequest'] = MockXMLHttpRequest;
  };

  function uninstallMockXMLHttpRequest() {
    window['XMLHttpRequest'] = originalXMLHttpRequest;
  };

  exports.installMockXMLHttpRequest = installMockXMLHttpRequest;
  exports.uninstallMockXMLHttpRequest = uninstallMockXMLHttpRequest;

})(window);
