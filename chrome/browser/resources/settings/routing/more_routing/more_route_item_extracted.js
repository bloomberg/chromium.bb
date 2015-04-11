// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

    Polymer('more-route-item', {
      publish: {
        /**
         * The name of this route.
         *
         * @attribute name
         * @type String
         */
        name: '',

        /**
         * A path expression used to parse parameters from the window's URL.
         *
         * @attribute path
         * @type String
         */
        path: '',

        /**
         * Param values to use for navigating to this item, and for determining
         * whether the item is `current`.
         *
         * @attribute params
         * @type Object
         */
        params: {},

        /**
         * Whether the route matches the current URL.
         *
         * @attribute active
         * @type Boolean
         * @readonly
         */
        active: {value: false, reflect: true},

        /**
         * Whether the route _and params_ match the current URL.
         *
         * @attribute current
         * @type Boolean
         * @readonly
         */
        current: {value: false, reflect: true},
      },

      observe: {
        '$.route.route.parts': '_updateCurrent',
      },

      _updateCurrent: function() {
        this.current = this.$.route.isCurrentUrl(this.params);
      },

      _onTap: function() {
        this.$.route.navigateTo(this.params);
      },
    });
  
