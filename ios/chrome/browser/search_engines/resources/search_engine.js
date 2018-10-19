// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Add functionality related to getting image data.
 */
goog.provide('__crWeb.searchEngine');

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.searchEngine = {};

/**
 * Store common namespace object in a global __gCrWeb object referenced by a
 * string, so it does not get renamed by closure compiler during the
 * minification.
 */
__gCrWeb['searchEngine'] = __gCrWeb.searchEngine;

/* Beginning of anonymous object. */
(function() {

/**
 * Find <link> of OSDD(Open Search Description Document) in document and return
 * it's URL. If multiple OSDDs are found(which should never happen on a sane web
 * site), return the URL of the first OSDD.
 * @return {string|undefined} "href" of OSDD <link>, or undefined if not found.
 */
__gCrWeb.searchEngine.getOpenSearchDescriptionDocumentUrl = function() {
  var links = document.getElementsByTagName('link');
  for (var i = 0; i < links.length; ++i) {
    if (links[i].type == 'application/opensearchdescription+xml')
      return links[i].href;
  }
};

}());  // End of anonymous object
