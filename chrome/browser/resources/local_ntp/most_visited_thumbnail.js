// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Rendering for iframed most visited thumbnails.
 */

window.addEventListener('DOMContentLoaded', function() {
  'use strict';

  fillMostVisited(document.location, function(params, data) {
    function showDomainElement() {
      var link = createMostVisitedLink(params, data.url, data.title);
      var domain = document.createElement('div');
      domain.textContent = data.domain;
      link.appendChild(domain);
      document.body.appendChild(link);
    };
    if (data.thumbnailUrl) {
      var image = new Image();
      image.onload = function() {
        var link = createMostVisitedLink(params, data.url, data.title);
        link.appendChild(image);
        document.body.appendChild(link);
      };

      image.onerror = showDomainElement;
      image.src = data.thumbnailUrl;
    } else {
      showDomainElement();
    }
  });
});
