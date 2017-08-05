// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  class ActionService {
    constructor() {
      /** @private {Array<string>} */
      this.searchTerms_ = [];
    }

    /**
     * @param {string} searchText Input typed by the user into a search box.
     * @return {Array<string>} A list of terms extracted from |searchText|.
     */
    static splitTerms(searchText) {
      // Split quoted terms (e.g., 'The "lazy" dog' => ['The', 'lazy', 'dog']).
      return searchText.split(/"([^"]*)"/).map(s => s.trim()).filter(s => !!s);
    }

    /** @param {string} id ID of the download to cancel. */
    cancel(id) {
      chrome.send('cancel', [id]);
    }

    /** Instructs the browser to clear all finished downloads. */
    clearAll() {
      if (loadTimeData.getBoolean('allowDeletingHistory')) {
        chrome.send('clearAll');
        this.search('');
      }
    }

    /** @param {string} id ID of the dangerous download to discard. */
    discardDangerous(id) {
      chrome.send('discardDangerous', [id]);
    }

    /** @param {string} url URL of a file to download. */
    download(url) {
      const a = document.createElement('a');
      a.href = url;
      a.setAttribute('download', '');
      a.click();
    }

    /** @param {string} id ID of the download that the user started dragging. */
    drag(id) {
      chrome.send('drag', [id]);
    }

    /** Loads more downloads with the current search terms. */
    loadMore() {
      chrome.send('getDownloads', this.searchTerms_);
    }

    /**
     * @return {boolean} Whether the user is currently searching for downloads
     *     (i.e. has a non-empty search term).
     */
    isSearching() {
      return this.searchTerms_.length > 0;
    }

    /** Opens the current local destination for downloads. */
    openDownloadsFolder() {
      chrome.send('openDownloadsFolderRequiringGesture');
    }

    /**
     * @param {string} id ID of the download to run locally on the user's box.
     */
    openFile(id) {
      chrome.send('openFileRequiringGesture', [id]);
    }

    /** @param {string} id ID the of the progressing download to pause. */
    pause(id) {
      chrome.send('pause', [id]);
    }

    /** @param {string} id ID of the finished download to remove. */
    remove(id) {
      chrome.send('remove', [id]);
    }

    /** @param {string} id ID of the paused download to resume. */
    resume(id) {
      chrome.send('resume', [id]);
    }

    /**
     * @param {string} id ID of the dangerous download to save despite
     *     warnings.
     */
    saveDangerous(id) {
      chrome.send('saveDangerousRequiringGesture', [id]);
    }

    /**
     * @param {string} searchText What to search for.
     * @return {boolean} Whether |searchText| resulted in new search terms.
     */
    search(searchText) {
      const searchTerms = ActionService.splitTerms(searchText);
      let sameTerms = searchTerms.length == this.searchTerms_.length;

      for (let i = 0; sameTerms && i < searchTerms.length; ++i) {
        if (searchTerms[i] != this.searchTerms_[i])
          sameTerms = false;
      }

      if (sameTerms)
        return false;

      this.searchTerms_ = searchTerms;
      this.loadMore();
      return true;
    }

    /**
     * Shows the local folder a finished download resides in.
     * @param {string} id ID of the download to show.
     */
    show(id) {
      chrome.send('show', [id]);
    }

    /** Undo download removal. */
    undo() {
      chrome.send('undo');
    }
  }

  cr.addSingletonGetter(ActionService);

  return {ActionService: ActionService};
});
