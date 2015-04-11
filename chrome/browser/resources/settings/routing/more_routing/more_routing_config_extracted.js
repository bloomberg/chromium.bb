// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

    Polymer('more-routing-config', {
      publish: {
        /**
         * The routing driver to use.
         *
         *  * `hash`: `MoreRouting.HashDriver`
         *  * `path`: `MoreRouting.PathDriver`
         *  * `mock`: `MoreRouting.MockDriver`
         *
         * @attribute driver
         * @type String
         */
        driver: '',

        /**
         *
         */
        urlPrefix: '',
      },

      ready: function() {
        var driver;
        // TODO(nevir): Support custom drivers, too.
        if (this.driver === 'hash') {
          driver = new MoreRouting.HashDriver(this._config);
        } else if (this.driver === 'path') {
          driver = new MoreRouting.PathDriver(this._config);
        } else if (this.driver === 'mock') {
          driver = new MoreRouting.MockDriver(this._config);
        } else {
          throw new Error('Unknown driver type "' + this.driver + '"');
        }

        MoreRouting.driver = driver;
      },

      get _config() {
        var config = {};
        if (this.urlPrefix) config.prefix = this.urlPrefix;
        return config;
      },
    });
  
