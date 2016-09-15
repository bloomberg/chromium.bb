// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('test_util', function() {
  /**
   * Observes an HTML attribute and fires a promise when it matches a given
   * value.
   * @param {!HTMLElement} target
   * @param {string} attributeName
   * @param {*} attributeValue
   * @return {!Promise}
   */
  function whenAttributeIs(target, attributeName, attributeValue) {
    function isDone() { return target[attributeName] === attributeValue; }

    return isDone() ? Promise.resolve() : new Promise(function(resolve) {
      new MutationObserver(function(mutations, observer) {
        for (var mutation of mutations) {
          assertEquals('attributes', mutation.type);
          if (mutation.attributeName == attributeName && isDone()) {
            observer.disconnect();
            resolve();
            return;
          }
        }
      }).observe(
          target, {attributes: true, childList: false, characterData: false});
    });
  }

  /**
   * Converts an event occurrence to a promise.
   * @param {string} eventType
   * @param {!HTMLElement} target
   * @return {!Promise} A promise firing once the event occurs.
   */
  function eventToPromise(eventType, target) {
    return new Promise(function(resolve, reject) {
      target.addEventListener(eventType, resolve);
    });
  }

  return {
    eventToPromise: eventToPromise,
    whenAttributeIs: whenAttributeIs,
  };
});
