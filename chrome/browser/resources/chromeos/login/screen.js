// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for all login WebUI screens.
 */
cr.define('login', function() {
  var Screen = cr.ui.define('div');

  Screen.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
    }
  };

  return {
    Screen: Screen
  };
});

cr.define('login', function() {
  return {
    /**
     * Creates class and object for screen.
     * Methods specified in EXTERNAL_API array of prototype
     * will be available from C++ part.
     * Example:
     *     login.createScreen('ScreenName', 'screen-id', {
     *       foo: function() { console.log('foo'); },
     *       bar: function() { console.log('bar'); }
     *       EXTERNAL_API: ['foo'];
     *     });
     *     login.ScreenName.register();
     *     var screen = $('screen-id');
     *     screen.foo(); // valid
     *     login.ScreenName.foo(); // valid
     *     screen.bar(); // valid
     *     login.ScreenName.bar(); // invalid
     *
     * @param {string} name Name of created class.
     * @param {string} id Id of div representing screen.
     * @param {(function()|Object)} proto Prototype of object or function that
     *     returns prototype.
     */
    createScreen: function(name, id, proto) {
      if (typeof proto == 'function')
        proto = proto();
      cr.define('login', function() {
        var api = proto.EXTERNAL_API || [];
        for (var i = 0; i < api.length; ++i) {
          var methodName = api[i];
          if (typeof proto[methodName] !== 'function')
            throw Error('External method "' + methodName + '" for screen "' +
                name + '" not a function or undefined.');
        }

        var constructor = cr.ui.define(login.Screen);
        constructor.prototype = Object.create(login.Screen.prototype);
        Object.getOwnPropertyNames(proto).forEach(function(propertyName) {
          var descriptor =
              Object.getOwnPropertyDescriptor(proto, propertyName);
          Object.defineProperty(constructor.prototype,
              propertyName, descriptor);
          if (api.indexOf(propertyName) >= 0) {
            constructor[propertyName] = (function(x) {
              return function() {
                var screen = $(id);
                return screen[x].apply(screen, arguments);
              };
            })(propertyName);
          }
        });
        constructor.prototype.name = function() { return id; };

        constructor.register = function() {
          var screen = $(id);
          constructor.decorate(screen);
          Oobe.getInstance().registerScreen(screen);
        };

        var result = {};
        result[name] = constructor;
        return result;
      });
    }
  };
});

