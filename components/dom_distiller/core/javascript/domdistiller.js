// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These includes will be processed at build time by grit.
<include src="../../../../third_party/dom_distiller_js/package/js/domdistiller.js"/>

// Extracts long-form content from a page and returns an array where the first
// element is the article title, the second element is HTML containing the
// long-form content, the third element is the next page link, and the fourth
// element is the previous page link.
(function() {
  var result = new Array(4);
  try {
    result[0] = com.dom_distiller.DocumentTitleGetter.getDocumentTitle(
        document.title, document.documentElement);
    result[1] = com.dom_distiller.ContentExtractor.extractContent();
    result[2] = com.dom_distiller.PagingLinksFinder.findNext(
        document.documentElement);
    // TODO(shashishekhar): Add actual previous page link here.
    result[3] = '';

    /**
     * // Properties from markup tags.
     *
     * var parser = new com.dom_distiller.MarkupParser(document.documentElement);
     * if (!parser.optOut()) {
     *   // Basic properties.
     *   var title = parser.getTitle();  // String type.
     *   var pageType = parser.getType();  // String type.
     *   var pageUrl = parser.getUrl();  // String type.
     *   var author = parser.getAuthor();  // String type.
     *   var description = parser.getDescription();  // String type.
     *   var publisher = parser.getPublisher();  // String type.
     *   var copyright = parser.getCopyright();  // String type.
     *
     *   // Structured Image's.
     *   var images = parser.getImages();
     *   for (var i = 0; i < images.length; i++) {
     *     var image = images[i];
     *     var url = image.getUrl();  // String type.
     *     var secureUrl = image.getSecureUrl();  // String type.
     *     var type = image.getType();  // String type.
     *     var caption = image.getCaption();  // String type.
     *     var width = image.getWidth();  // int type.
     *     var height = image.getHeight();  // int type.
     *   }
     *
     *   // Structured Article.
     *   var article = parser.getArticle();
     *   if (article != null) {
     *     var publishedTime = article.getPublishedTime();  // String type.
     *     var modifiedTime = article.getModifiedTime();  // String type.
     *     var expirationTime = article.getExpirationTime();  // String type.
     *     var sectionName = article.getSection();  // String type.
     *     var authors = article.getAuthors();
     *     for (var i = 0; i < authors.length; i++) {
     *       var author = authors[i];  // String type.
     *     }
     *   }
     * }
     */

  } catch (e) {
    window.console.log("Error during distillation: " + e);
  }
  return result;
})()
