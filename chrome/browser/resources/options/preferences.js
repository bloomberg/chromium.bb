// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  /////////////////////////////////////////////////////////////////////////////
  // Preferences class:

  /**
   * Preferences class manages access to Chrome profile preferences.
   * @constructor
   */
  function Preferences() {
  }

  cr.addSingletonGetter(Preferences);

  /**
   * Extracts preference value.
   * @param {Object} dict Map of preference values passed to fetchPrefs
   *     callback.
   * @param {string} name Preference name.
   * @return preference value.
   */
  Preferences.getPref = function (dict, name) {
    var parts = name.split('.');
    var cur = dict;
    for (var part; part = parts.shift(); ) {
      if (cur[part]) {
        cur = cur[part];
      } else {
        return null;
      }
    }
    return cur;
  };

  /**
   * Sets value of a boolean preference.
   * and signals its changed value.
   * @param {string} name Preference name.
   * @param {boolean} value New preference value.
   * @param {string} metric User metrics identifier.
   */
  Preferences.setBooleanPref = function (name, value, metric) {
    var argumentList = [name, Boolean(value)];
    if (metric != undefined) argumentList.push(metric);
    chrome.send('setBooleanPref', argumentList);
  };

  /**
   * Sets value of an integer preference.
   * and signals its changed value.
   * @param {string} name Preference name.
   * @param {number} value New preference value.
   * @param {string} metric User metrics identifier.
   */
  Preferences.setIntegerPref = function(name, value, metric) {
    var argumentList = [name, Number(value)];
    if (metric != undefined) argumentList.push(metric);
    chrome.send('setIntegerPref', argumentList);
  };

  /**
   * Sets value of a double-valued preference.
   * and signals its changed value.
   * @param {string} name Preference name.
   * @param {number} value New preference value.
   * @param {string} metric User metrics identifier.
   */
  Preferences.setDoublePref = function(name, value, metric) {
    var argumentList = [name, Number(value)];
    if (metric != undefined) argumentList.push(metric);
    chrome.send('setDoublePref', argumentList);
  };

  /**
   * Sets value of a string preference.
   * and signals its changed value.
   * @param {string} name Preference name.
   * @param {string} value New preference value.
   * @param {string} metric User metrics identifier.
   */
  Preferences.setStringPref = function(name, value, metric) {
    var argumentList = [name, String(value)];
    if (metric != undefined) argumentList.push(metric);
    chrome.send('setStringPref', argumentList);
  };

  /**
   * Sets value of a JSON preference.
   * and signals its changed value.
   * @param {string} name Preference name.
   * @param {*} value New preference value.
   * @param {string} metric User metrics identifier.
   */
  Preferences.setObjectPref = function(name, value, metric) {
    var argumentList = [name, JSON.stringify(value)];
    if (metric != undefined) argumentList.push(metric);
    chrome.send('setObjectPref', argumentList);
  };

  /**
   * Clears value of a JSON preference.
   * @param {string} name Preference name.
   * @param {string} metric User metrics identifier.
   */
  Preferences.clearPref = function(name, metric) {
    var argumentList = [name];
    if (metric != undefined) argumentList.push(metric);
    chrome.send('clearPref', argumentList);
  };

  Preferences.prototype = {
    __proto__: cr.EventTarget.prototype,

    // Map of registered preferences.
    registeredPreferences_: {},

    /**
     * Adds an event listener to the target.
     * @param {string} type The name of the event.
     * @param {!Function|{handleEvent:Function}} handler The handler for the
     *     event. This is called when the event is dispatched.
     */
    addEventListener: function(type, handler) {
      cr.EventTarget.prototype.addEventListener.call(this, type, handler);
      this.registeredPreferences_[type] = true;
    },

    /**
     * Initializes preference reading and change notifications.
     */
    initialize: function() {
      var params1 = ['Preferences.prefsFetchedCallback'];
      var params2 = ['Preferences.prefsChangedCallback'];
      for (var prefName in this.registeredPreferences_) {
        params1.push(prefName);
        params2.push(prefName);
      }
      chrome.send('fetchPrefs', params1);
      chrome.send('observePrefs', params2);
    },

    /**
     * Helper function for flattening of dictionary passed via fetchPrefs
     * callback.
     * @param {string} prefix Preference name prefix.
     * @param {object} dict Map with preference values.
     */
    flattenMapAndDispatchEvent_: function(prefix, dict) {
      for (var prefName in dict) {
        if (typeof dict[prefName] == 'object' &&
            !this.registeredPreferences_[prefix + prefName]) {
          this.flattenMapAndDispatchEvent_(prefix + prefName + '.',
              dict[prefName]);
        } else {
          var event = new cr.Event(prefix + prefName);
          event.value = dict[prefName];
          this.dispatchEvent(event);
        }
      }
    }
  };

  /**
   * Callback for fetchPrefs method.
   * @param {object} dict Map of fetched property values.
   */
  Preferences.prefsFetchedCallback = function(dict) {
    Preferences.getInstance().flattenMapAndDispatchEvent_('', dict);
  };

  /**
   * Callback for observePrefs method.
   * @param {array} notification An array defining changed preference values.
   * notification[0] contains name of the change preference while its new value
   * is stored in notification[1].
   */
  Preferences.prefsChangedCallback = function(notification) {
    var event = new cr.Event(notification[0]);
    event.value = notification[1];
    Preferences.getInstance().dispatchEvent(event);
  };

  // Export
  return {
    Preferences: Preferences
  };

});
