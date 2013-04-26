// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This code is used in conjunction with the Google Translate Element script.
// It is injected in a page to translate it from one language to another.
// It should be included in the page before the Translate Element script.

var cr = {};

cr.googleTranslate = (function(key) {
  /**
   * The Translate Element library's instance.
   * @type {object}
   */
  var lib;

  /**
   * A flag representing if the Translate Element library is initialized.
   * @type {boolean}
   */
  var libReady = false;

  /**
   * A flag representing if the Translate Element library returns error while
   * performing translation. Also it is set to true when the Translate Element
   * library is not initialized in 600 msec from the library is loaded.
   * @type {boolean}
   */
  var error = false;

  /**
   * A flag representing if the Translate Element has finished a translation.
   * @type {boolean}
   */
  var finished = false;

  /**
   * Counts how many times the checkLibReady function is called. The function
   * is called in every 100 msec and counted up to 6.
   * @type {number}
   */
  var checkReadyCount = 0;

  /**
   * Time in msec when this script is injected.
   * @type {number}
   */
  var injectedTime = performance.now();

  /**
   * Time in msec when the Translate Element library is loaded completely.
   * @type {number}
   */
  var loadedTime = 0.0;

  /**
   * Time in msec when the Translate Element library is initialized and ready
   * for performing translation.
   * @type {number}
   */
  var readyTime = 0.0;

  /**
   * Time in msec when the Translate Element library starts a translation.
   * @type {number}
   */
  var startTime = 0.0;

  /**
   * Time in msec when the Translate Element library ends a translation.
   * @type {number}
   */
  var endTime = 0.0;

  function checkLibReady() {
    if (lib.isAvailable()) {
      readyTime = performance.now();
      libReady = true;
      return;
    }
    if (checkReadyCount++ > 5) {
      error = true;
      return;
    }
    setTimeout(checkLibReady, 100);
  }

  function onTranslateProgress(progress, opt_finished, opt_error) {
    finished = opt_finished;
    // opt_error can be 'undefined'.
    if (typeof opt_error == 'boolean' && opt_error) {
      error = true;
      // We failed to translate, restore so the page is in a consistent state.
      lib.restore();
    }
    if (finished)
      endTime = performance.now();
  }

  // Public API.
  return {
    /**
     * Whether the library is ready.
     * The translate function should only be called when |libReady| is true.
     * @type {boolean}
     */
    get libReady() {
      return libReady;
    },

    /**
     * Whether the current translate has finished successfully.
     * @type {boolean}
     */
    get finished() {
      return finished;
    },

    /**
     * Whether an error occured initializing the library of translating the
     * page.
     * @type {boolean}
     */
    get error() {
      return error;
    },

    /**
     * The language the page translated was in. Is valid only after the page
     * has been successfully translated and the original language specified to
     * the translate function was 'auto'. Is empty otherwise.
     * @type {boolean}
     */
    get sourceLang() {
      if (!libReady || !finished || error)
        return '';
      return lib.getDetectedLanguage();
    },

    /**
     * Time in msec from this script being injected to all server side scripts
     * being loaded.
     * @type {number}
     */
    get loadTime() {
      if (loadedTime == 0)
        return 0;
      return loadedTime - injectedTime;
    },

    /**
     * Time in msec from this script being injected to the Translate Element
     * library being ready.
     * @type {number}
     */
    get readyTime() {
      if (!libReady)
        return 0;
      return readyTime - injectedTime;
    },

    /**
     * Time in msec to perform translation.
     * @type {number}
     */
    get translationTime() {
      if (!finished)
        return 0;
      return endTime - startTime;
    },

    /**
     * Translate the page contents.  Note that the translation is asynchronous.
     * You need to regularly check the state of |finished| and |error| to know
     * if the translation finished or if there was an error.
     * @param {string} originalLang The language the page is in.
     * @param {string} targetLang The language the page should be translated to.
     * @return {boolean} False if the translate library was not ready, in which
     *                   case the translation is not started.  True otherwise.
     */
    translate: function(originalLang, targetLang) {
      finished = false;
      error = false;
      if (!libReady)
        return false;
      startTime = performance.now();
      lib.translatePage(originalLang, targetLang, onTranslateProgress);
      return true;
    },

    /**
     * Reverts the page contents to its original value, effectively reverting
     * any performed translation.  Does nothing if the page was not translated.
     */
    revert: function() {
      lib.restore();
    },

    /**
     * Entry point called by the Translate Element once it has been injected in
     * the page.
     */
    onTranslateElementLoad: function() {
      loadedTime = performance.now();
      try {
        lib = google.translate.TranslateService({'key': key});
      } catch (err) {
        error = true;
        return;
      }
      // The TranslateService is not available immediately as it needs to start
      // Flash.  Let's wait until it is ready.
      checkLibReady();
    }
  };
})/* Calling code '(|key|);' will be appended by TranslateHelper in C++ here. */
