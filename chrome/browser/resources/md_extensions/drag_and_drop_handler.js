// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * @param {boolean} dragEnabled
   * @param {boolean} isMdExtensions
   * @param {!EventTarget} target
   * @constructor
   * @implements cr.ui.DragWrapperDelegate
   */
  function DragAndDropHandler(dragEnabled, isMdExtensions, target) {
    this.dragEnabled = dragEnabled;

    // Behavior is different for dropped directories between MD and non-MD
    // extensions pages.
    // TODO(devlin): Delete the non-MD codepath and remove this variable when
    // MD extensions launches.
    /** @private {boolean} */
    this.isMdExtensions_ = isMdExtensions;

    /** @private {!EventTarget} */
    this.eventTarget_ = target;
  }

  // TODO(devlin): Finish un-chrome.send-ifying this implementation.
  DragAndDropHandler.prototype = {
    /** @override */
    shouldAcceptDrag: function(e) {
      // External Extension installation can be disabled globally, e.g. while a
      // different overlay is already showing.
      if (!this.dragEnabled)
        return false;

      // We can't access filenames during the 'dragenter' event, so we have to
      // wait until 'drop' to decide whether to do something with the file or
      // not.
      // See: http://www.w3.org/TR/2011/WD-html5-20110113/dnd.html#concept-dnd-p
      return !!e.dataTransfer.types &&
          e.dataTransfer.types.indexOf('Files') > -1;
    },

    /** @override */
    doDragEnter: function() {
      chrome.send('startDrag');
      if (this.isMdExtensions_)
        chrome.developerPrivate.notifyDragInstallInProgress();

      this.eventTarget_.dispatchEvent(
          new CustomEvent('extension-drag-started'));
    },

    /** @override */
    doDragLeave: function() {
      this.fireDragEnded_();
      chrome.send('stopDrag');
    },

    /** @override */
    doDragOver: function(e) {
      e.preventDefault();
    },

    /** @override */
    doDrop: function(e) {
      this.fireDragEnded_();
      if (e.dataTransfer.files.length != 1)
        return;

      let handled = false;

      // Files lack a check if they're a directory, but we can find out through
      // its item entry.
      let item = e.dataTransfer.items[0];
      if (item.kind === 'file' && item.webkitGetAsEntry().isDirectory) {
        handled = true;
        this.handleDirectoryDrop_();
      } else if (/\.(crx|user\.js|zip)$/i.test(e.dataTransfer.files[0].name)) {
        // Only process files that look like extensions. Other files should
        // navigate the browser normally.
        handled = true;
        this.handleFileDrop_();
      }

      if (handled)
        e.preventDefault();
    },

    /**
     * Handles a dropped file.
     * @private
     */
    handleFileDrop_: function() {
      // Packaged files always go through chrome.send (for now).
      chrome.send('installDroppedFile');
    },

    /**
     * Handles a dropped directory.
     * @private
     */
    handleDirectoryDrop_: function() {
      // Dropped directories either go through developerPrivate or chrome.send
      // depending on if this is the MD page.
      if (!this.isMdExtensions_) {
        chrome.send('installDroppedDirectory');
        return;
      }

      // TODO(devlin): Update this to use extensions.Service when it's not
      // shared between the MD and non-MD pages.
      chrome.developerPrivate.loadUnpacked(
          {failQuietly: true, populateError: true, useDraggedPath: true},
          (loadError) => {
            if (loadError) {
              this.eventTarget_.dispatchEvent(new CustomEvent(
                  'drag-and-drop-load-error', {detail: loadError}));
            }
          });
    },

    /** @private */
    fireDragEnded_: function() {
      this.eventTarget_.dispatchEvent(new CustomEvent('extension-drag-ended'));
    }
  };

  return {
    DragAndDropHandler: DragAndDropHandler,
  };
});
