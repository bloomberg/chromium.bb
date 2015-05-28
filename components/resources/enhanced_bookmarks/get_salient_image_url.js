// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.require('image.collections.extension.domextractor.DocumentImageExtractor');

/*
 * Examines the DOM to find the image that is the most representative of the
 * webpage.
 */
var GetSalientImageUrl = function() {
  // Extract the referrer policy from the page.
  var referrerPolicy = 'default';
  var metaTags = document.getElementsByTagName('meta');
  for (var i = 0; i < metaTags.length; i++) {
    if (metaTags[i].name.toLowerCase() == 'referrer') {
      referrerPolicy = metaTags[i].content.toLowerCase();
      break;
    }
  }

  // Determine which JSON method (stringify or encode) to use for encoding.
  // Some pages use a library that overrides JSON, so JSON['stringify'] is not
  // always available.
  var jsonEncoder = JSON['stringify'];
  if (!jsonEncoder)
    jsonEncoder = JSON['encode'];

  var context = new
      image.collections.extension.domextractor.DocumentImageExtractor.Context();
  var documentImageExtractor =
      new image.collections.extension.domextractor.DocumentImageExtractor();
  var images = documentImageExtractor.extractFromNodeList(
      document.querySelectorAll('img,link,meta'), context);

  var maxImage = null;
  var maxRelevance = 0;
  for (var i = 0; i < images.length; i++) {
    var currentImage = images[i];
    var imageSize = currentImage.getSize();
    if (!imageSize)
      continue;

    if (currentImage.getRelevance() > maxRelevance) {
      maxImage = currentImage;
      maxRelevance = currentImage.getRelevance();
    }
  }

  if (maxImage === null)
    return '';

  var result = jsonEncoder({
    'imageUrl': maxImage.getUrl(),
    'referrerPolicy': referrerPolicy
  });

  return result;
};
