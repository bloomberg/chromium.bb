// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This code is used in conjunction with the Google Translate Element script.
// It is injected in a page to translate it from one language to another.
// It should be included in the page before the Translate Element script.

var cr = {};

cr.googleTranslate = (function(key) {
  // Internal states.
  var lib;
  var libReady = false;
  var error = false;
  var finished = false;
  var checkReadyCount = 0;

  function checkLibReady() {
    if (lib.isAvailable()) {
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
