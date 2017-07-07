// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  /**
   * Test version of the print preview PDF plugin
   */
  class PDFPluginStub {
    /**
     * @param {!print_preview.PreviewArea} The PreviewArea that owns this
     *     plugin.
     */
    constructor(area) {
      /**
       * @private {?Function} The callback to run when the plugin has loaded.
       */
      this.loadCallback_ = null;

      /** @private {!EventTracker} The plugin stub's event tracker. */
      this.tracker_ = new EventTracker();

      // Call documentLoadComplete as soon as the preview area starts the
      // preview.
      this.tracker_.add(
          area,
          print_preview.PreviewArea.EventType.PREVIEW_GENERATION_IN_PROGRESS,
          this.documentLoadComplete.bind(this));
    }

    /**
     * @param {!Function} callback The callback to run when the plugin has
     *     loaded.
     */
    setLoadCallback(callback) {
      this.loadCallback_ = callback;
    }

    documentLoadComplete() {
      if (this.loadCallback_)
        this.loadCallback_();
    }

    /**
     * Stubbed out since some tests result in a call.
     * @param {string} url The url to initialize the plugin to.
     * @param {boolean} color Whether the preview should be in color.
     * @param {!Array<number>} pages The pages to preview.
     * @param {boolean} modifiable Whether the source document is modifiable.
     */
    resetPrintPreviewMode(url, color, pages, modifiable) {}
  }

  return {PDFPluginStub: PDFPluginStub};
});
