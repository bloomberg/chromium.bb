// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

export class TabsApiProxy {
  constructor() {
    /** @type {!Object<string, !ChromeEvent>} */
    this.callbackRouter = {
      onCreated: chrome.tabs.onCreated,
      onRemoved: chrome.tabs.onRemoved,
      onUpdated: chrome.tabs.onUpdated,
    };
  }

  /**
   * @return {!Promise<!ChromeWindow>}
   */
  getCurrentWindow() {
    const options = {
      populate: true,           // populate window data with tabs data
      windowTypes: ['normal'],  // prevent devtools from being returned
    };
    return new Promise(resolve => {
      chrome.windows.getCurrent(options, currentWindow => {
        resolve(currentWindow);
      });
    });
  }
}

addSingletonGetter(TabsApiProxy);
