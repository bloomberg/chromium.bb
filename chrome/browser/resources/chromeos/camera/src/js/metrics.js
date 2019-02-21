// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for metrics.
 */
cca.metrics = cca.metrics || {};

/**
 * Event builder for basic metrics.
 * @type {analytics.EventBuilder}
 * @private
 */
cca.metrics.base_ = null;

/**
 * Promise for Google Analytics tracker.
 * @type {Promise<analytics.Tracker>}
 * @private
 */
cca.metrics.ga_ = (function() {
  const analytics = window['analytics'] || {};
  const id = 'UA-134822711-2';  // TODO(yuli): Use prod id.
  const service = analytics.getService('chrome-camera-app');

  var getConfig = () =>
      new Promise((resolve) => service.getConfig().addCallback(resolve));
  var checkEnabled = () => {
    return new Promise((resolve) => {
      try {
        chrome.metricsPrivate.getIsCrashReportingEnabled(resolve);
      } catch (e) {
        resolve(false);  // Disable reporting by default.
      }
    });
  };
  var initBuilder = () => {
    return new Promise((resolve) => {
      try {
        chrome.chromeosInfoPrivate.get(['board'],
            (values) => resolve(values['board']));
      } catch (e) {
        resolve('');
      }
    }).then((board) => {
      var boardName = /^(x86-)?(\w*)/.exec(board)[0];
      var match = navigator.appVersion.match(/CrOS\s+\S+\s+([\d.]+)/);
      var osVer = match ? match[1] : '';
      cca.metrics.base_ = analytics.EventBuilder.builder()
          .dimension(1, boardName).dimension(2, osVer);
    });
  };

  return Promise.all([getConfig(), checkEnabled(), initBuilder()])
      .then(([config, enabled]) => {
        config.setTrackingPermitted(enabled);
        return service.getTracker(id);
      });
})();

/**
 * Tries to fetch the given states and returns the first existing state.
 * @param {Array<string>} states States to be fetched.
 * @return {string} The first existing state among the given states.
 * @private
 */
cca.metrics.state_ = function(states) {
  return states.find((state) => cca.state.get(state)) || '';
};

/**
 * Returns event builder for the metrics type: launch.
 * @return {analytics.EventBuilder}
 * @private
 */
cca.metrics.launchType_ = function() {
  return cca.metrics.base_.category('launch').action('launch-app')
      .label(cca.metrics.state_(['migrate-prompted']));
};

/**
 * Metrics types.
 * @enum {function(*=)}
 */
cca.metrics.Type = {
  LAUNCH: cca.metrics.launchType_,
};

/**
 * Logs the given metrics.
 * @param {cca.metrics.Type} type Metrics type.
 * @param {...*} args Optional rest parameters for logging metrics.
 */
cca.metrics.log = function(type, ...args) {
  cca.metrics.ga_.then((tracker) => tracker.send(type(...args)));
};
