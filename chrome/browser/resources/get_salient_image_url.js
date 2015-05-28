// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Looks on the page to find the image that could be the most representative
 * one.
 */
(function() {
  // Extract the referrer policy from the page.
  var referrerPolicy = 'default';
  var metaTags = document.getElementsByTagName('meta');
  for (var i = 0; i < metaTags.length; ++i) {
    if (metaTags[i].name.toLowerCase() == 'referrer') {
      referrerPolicy = metaTags[i].content.toLowerCase();
      break;
    }
  }

  // See what to use for JSON. Some pages use a library that overrides JSON.
  var jsonEncoder = JSON.stringify;
  if (!jsonEncoder)
    jsonEncoder = JSON.encode;

  // First look if there is an Open Graph Image property available.
  var ogImage = document.querySelector('meta[property=\"og:image\"]');
  if (ogImage) {
    // Checks that the url in ogImage has a path that contains more than just a
    // simple '/'.
    var url = ogImage.content;
    var location = document.createElement('a');
    location.href = url;
    if (location.pathname.length > 1) {
      return jsonEncoder({
        'imageUrl': url,
        'referrerPolicy': referrerPolicy
      });
    }
  }

  // Iterates through the images on the page, find the largest one that doesn't
  // look like a banner.
  var maxPointSize = 0;
  var maxImage = null;

  var images = document.getElementsByTagName('img');
  for (var i = 0; i < images.length; i++) {
    var currentImage = images[i];
    var aspectRatio = currentImage.width / currentImage.height;
    if (aspectRatio >= 2.4 || aspectRatio <= 0.5)
      continue;  // Skip weirdly shaped images. Those are ads or headers.
    var pointSize = currentImage.width * currentImage.height;
    if (pointSize > maxPointSize) {
      maxPointSize = pointSize;
      maxImage = currentImage;
    }
  }

  // Only keep images larger than 320*160.
  if (maxPointSize <= 51200.0 || maxImage === null)
    return '';

  return jsonEncoder({
    'imageUrl': maxImage.src,
    'referrerPolicy': referrerPolicy
  });
})();
