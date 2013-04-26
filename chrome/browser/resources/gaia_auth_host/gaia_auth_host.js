// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An UI component to host gaia auth extension in an iframe.
 * After the component binds with an iframe, call its {@code load} to start the
 * authentication flow. There are two events would be raised after this point:
 * a 'ready' event when the authentication UI is ready to use and a 'completed'
 * event when the authentication is completed successfully. If caller is
 * interested in the user credentials, he may supply a success callback with
 * {@code load} call. The callback will be invoked when the authentication is
 * completed successfully and with the available credential data.
 */

cr.define('cr.login', function() {
  'use strict';

  /**
   * Base URL of gaia auth extension.
   * @const
   */
  var AUTH_URL_BASE = 'chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/';

  /**
   * Auth URL to use for online flow.
   * @const
   */
  var AUTH_URL = AUTH_URL_BASE + 'main.html';

  /**
   * Auth URL to use for offline flow.
   * @const
   */
  var OFFLINE_AUTH_URL = AUTH_URL_BASE + 'offline.html';

  /**
   * Supported params of auth extension. For a complete list, check out the
   * auth extension's main.js.
   * @type {!Array.<string>}
   * @const
   */
  var SUPPORTED_PARAMS = [
    'gaiaOrigin',    // Gaia origin to use;
    'gaiaUrlPath',   // Url path to gaia sign-in servlet on gaiaOrigin;
    'hl',            // Language code for the user interface;
    'email',         // Pre-fill the email field in Gaia UI;
    'test_email',    // Used for test;
    'test_password'  // Used for test;
  ];

  /**
   * Supported localized strings. For a complete list, check out the auth
   * extension's offline.js
   * @type {!Array.<string>}
   * @const
   */
  var LOCALIZED_STRING_PARAMS = [
      'stringSignIn',
      'stringEmail',
      'stringPassword',
      'stringEmptyEmail',
      'stringEmptyPassword',
      'stringError'
  ];

  /**
   * Creates a new gaia auth extension host.
   * @param {HTMLIFrameElement|string} container The iframe element or its id
   *     to host the auth extension.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function GaiaAuthHost(container) {
    this.frame_ = typeof container == 'string' ? $(container) : container;
    assert(this.frame_);
    window.addEventListener('message',
                            this.onMessage_.bind(this), false);
  }

  GaiaAuthHost.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * An url to use with {@code reload}.
     * @type {?string}
     * @private
     */
    reloadUrl_: null,

    /**
     * Invoked when authentication is completed successfully with credential
     * data. A credential data object looks like this:
     * <pre>
     * {@code
     * {
     *   email: 'xx@gmail.com',
     *   password: 'xxxx',  // May not present
     *   authCode: 'x/xx',  // May not present
     *   useOffline: false  // Whether the authentication uses the offline flow.
     * }
     * }
     * </pre>
     * @type {function(Object)}
     * @private
     */
    successCallback_: null,

    /**
     * The iframe container.
     * @type {HTMLIFrameElement}
     */
    get frame() {
      return this.frame_;
    },

    /**
     * Loads the auth extension.
     * @param {boolean} useOffline Whether to use offline url or not.
     * @param {Object} data Parameters for the auth extension. See the auth
     *     extension's main.js for all supported params and their defaults.
     * @param {function(Object)} successCallback A function to be called when
     *     the authentication is completed successfully. The callback is
     *     invoked with a credential object.
     */
    load: function(useOffline, data, successCallback) {
      var params = [];

      var populateParams = function(nameList, values) {
        if (!values)
          return;

        for (var i in nameList) {
          var name = nameList[i];
          if (values[name])
            params.push(name + '=' + encodeURIComponent(values[name]));
        }
      };

      populateParams(SUPPORTED_PARAMS, data);
      populateParams(LOCALIZED_STRING_PARAMS, data.localizedStrings);
      params.push('parentPage=' + encodeURIComponent(window.location.origin));

      var url = useOffline ? OFFLINE_AUTH_URL : AUTH_URL;
      url += '?' + params.join('&');

      this.frame_.src = url;
      this.reloadUrl_ = url;
      this.successCallback_ = successCallback;
    },

    /**
     * Reloads the auth extension.
     */
    reload: function() {
      this.frame_.src = this.reloadUrl_;
    },

    /**
     * Invoked to process authentication success.
     * @param {Object} credentials Credential object to pass to success
     *     callback.
     * @private
     */
    onAuthSuccess_: function(credentials) {
      if (this.successCallback_)
        this.successCallback_(credentials);
      cr.dispatchSimpleEvent(this, 'completed');
    },

    /**
     * Checks if message comes from the loaded authentication extension.
     * @param {Object} e Payload of the received HTML5 message.
     * @type {boolean}
     */
    isAuthExtMessage_: function(e) {
      return this.frame_.src &&
          this.frame_.src.indexOf(e.origin) == 0 &&
          e.source == this.frame_.contentWindow;
    },

    /**
     * Event handler that is invoked when HTML5 message is received.
     * @param {object} e Payload of the received HTML5 message.
     */
    onMessage_: function(e) {
      if (!this.isAuthExtMessage_(e))
        return;

      var msg = e.data;

      if (msg.method == 'loginUILoaded') {
        cr.dispatchSimpleEvent(this, 'ready');
        return;
      }

      if (!/^complete(Login|Authentication)$|^offlineLogin$/.test(msg.method))
        return;

      this.onAuthSuccess_({email: msg.email,
                           password: msg.password,
                           authCode: msg.authCode,
                           useOffline: msg.method == 'offlineLogin'});
    }
  };

  GaiaAuthHost.SUPPORTED_PARAMS = SUPPORTED_PARAMS;
  GaiaAuthHost.LOCALIZED_STRING_PARAMS = LOCALIZED_STRING_PARAMS;

  return {
    GaiaAuthHost: GaiaAuthHost
  };
});
