// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `cr-radio-group` wraps a radio-group and set of radio-buttons that control
 *  a supplied preference.
 *
 * Example:
 *      <settings-radio-group pref="{{prefs.settings.foo}}"
 *          label="Foo Options." buttons="{{fooOptionsList}}">
 *      </settings-radio-group>
 *
 * @element settings-radio-group
 */
Polymer({
  is: 'settings-radio-group',

  properties: {
    /**
     * The preference object to control.
     * @type {chrome.settingsPrivate.PrefObject|undefined}
     */
    pref: {
      type: Object,
      notify: true,
      observer: 'prefChanged_'
    },

    /**
     * IronSelectableBehavior selected attribute
     */
    selected: {
      type: String,
      observer: 'selectedChanged_'
    },
  },

  /** @private */
  prefChanged_: function() {
    if (!this.pref)
      return;
    if (this.pref.type == chrome.settingsPrivate.PrefType.NUMBER ||
        this.pref.type == chrome.settingsPrivate.PrefType.BOOLEAN) {
      this.selected = this.pref.value.toString();
    } else {
      assert(this.pref.type != chrome.settingsPrivate.PrefType.LIST);
      this.selected = /** @type {string} */(this.pref.value);
    }
  },

  /** @private */
  selectedChanged_: function() {
    if (!this.pref)
      return;
    if (this.pref.type == chrome.settingsPrivate.PrefType.NUMBER) {
      var n = parseInt(this.selected, 10);
      if (isNaN(n)) {
        console.error('Bad selected name for numerical pref: ' + this.selected);
        return;
      }
      this.set('pref.value', n);
    } else if (this.pref.type == chrome.settingsPrivate.PrefType.BOOLEAN) {
      this.set('pref.value', this.selected == 'true');
    } else {
      assert(this.pref.type != chrome.settingsPrivate.PrefType.LIST);
      this.set('pref.value', this.selected);
    }
  },
});
