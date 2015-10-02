// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  /**
   * @param {string} chromeSendName
   * @return {function(string):void} A chrome.send() callback with curried name.
   */
  function chromeSendWithId(chromeSendName) {
    return function(id) { chrome.send(chromeSendName, [id]); };
  }

  /** @constructor */
  function ActionService() {}

  ActionService.prototype = {
    /** @param {string} id ID of the download to cancel. */
    cancel: chromeSendWithId('cancel'),

    /** Instructs the browser to clear all finished downloads. */
    clearAll: function() {
      if (loadTimeData.getBoolean('allowDeletingHistory')) {
        chrome.send('clearAll');
        this.search('');
      }
    },

    /** @param {string} id ID of the dangerous download to discard. */
    discardDangerous: chromeSendWithId('discardDangerous'),

    /** @param {string} url URL of a file to download. */
    download: function(url) {
      var a = document.createElement('a');
      a.href = url;
      a.setAttribute('download', '');
      a.click();
    },

    /** @param {string} id ID of the download that the user started dragging. */
    drag: chromeSendWithId('drag'),

    /**
     * @return {boolean} Whether the user is currently searching for downloads
     *     (i.e. has a non-empty search term).
     */
    isSearching: function() {
      return this.searchText_.length > 0;
    },

    /** Opens the current local destination for downloads. */
    openDownloadsFolder: chrome.send.bind(chrome, 'openDownloadsFolder'),

    /**
     * @param {string} id ID of the download to run locally on the user's box.
     */
    openFile: chromeSendWithId('openFile'),

    /** @param {string} id ID the of the progressing download to pause. */
    pause: chromeSendWithId('pause'),

    /** @param {string} id ID of the finished download to remove. */
    remove: chromeSendWithId('remove'),

    /** @param {string} id ID of the paused download to resume. */
    resume: chromeSendWithId('resume'),

    /**
     * @param {string} id ID of the dangerous download to save despite
     *     warnings.
     */
    saveDangerous: chromeSendWithId('saveDangerous'),

    /** @param {string} searchText What to search for. */
    search: function(searchText) {
      if (this.searchText_ == searchText)
        return;

      this.searchText_ = searchText;

      // Split quoted terms (e.g., 'The "lazy" dog' => ['The', 'lazy', 'dog']).
      function trim(s) { return s.trim(); }
      chrome.send('getDownloads', searchText.split(/"([^"]*)"/).map(trim));
    },

    /**
     * Shows the local folder a finished download resides in.
     * @param {string} id ID of the download to show.
     */
    show: chromeSendWithId('show'),

    /** Undo download removal. */
    undo: chrome.send.bind(chrome, 'undo'),
  };

  cr.addSingletonGetter(ActionService);

  return {ActionService: ActionService};
});
