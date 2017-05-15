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
    function isDone() {
      return target.getAttribute(attributeName) == attributeValue;
    }

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

  /**
   * Data-binds two Polymer properties using the property-changed events and
   * set/notifyPath API. Useful for testing components which would normally be
   * used together.
   * @param {!HTMLElement} el1
   * @param {!HTMLElement} el2
   * @param {string} property
   */
  function fakeDataBind(el1, el2, property) {
    var forwardChange = function(el, event) {
      if (event.detail.hasOwnProperty('path'))
        el.notifyPath(event.detail.path, event.detail.value);
      else
        el.set(property, event.detail.value);
    };
    // Add the listeners symmetrically. Polymer will prevent recursion.
    el1.addEventListener(property + '-changed', forwardChange.bind(null, el2));
    el2.addEventListener(property + '-changed', forwardChange.bind(null, el1));
  }

  return {
    eventToPromise: eventToPromise,
    fakeDataBind: fakeDataBind,
    whenAttributeIs: whenAttributeIs,
  };
});
