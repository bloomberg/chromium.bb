// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Rendering for iframed most visited thumbnails.
 */

window.addEventListener('DOMContentLoaded', function() {
  'use strict';

  fillMostVisited(document.location, function(params, data) {
    function logEvent(eventName) {
      chrome.embeddedSearch.newTabPage.logEvent(eventName);
    }
    function showDomainElement() {
      logEvent(NTP_LOGGING_EVENT_TYPE.NTP_THUMBNAIL_ERROR);
      var link = createMostVisitedLink(params, data.url, data.title);
      var domain = document.createElement('div');
      domain.textContent = data.domain;
      link.appendChild(domain);
      document.body.appendChild(link);
    };
    function createAndAppendThumbnail(isVisible) {
      var image = new Image();
      image.onload = function() {
        var shadow = document.createElement('span');
        shadow.classList.add('shadow');
        var link = createMostVisitedLink(params, data.url, data.title);
        link.appendChild(shadow);
        link.appendChild(image);
        document.body.appendChild(link);
      };
      if (!isVisible) {
        image.style.visibility = 'hidden';
      }
      return image;
    }

    if (data.thumbnailUrl) {
      var image = createAndAppendThumbnail(true);
      // If a backup thumbnail URL was provided, preload it in case the first
      // thumbnail errors. The backup thumbnail is always preloaded so that the
      // server can't gain knowledge on the local thumbnail DB by specifying a
      // second URL that is only sometimes fetched.
      if (data.thumbnailUrl2) {
        var image2 = createAndAppendThumbnail(false);
        image2.onerror = showDomainElement;
        image2.src = data.thumbnailUrl2;
        // The first thumbnail's onerror function will swap the visibility of
        // the two thumbnails.
        image.onerror = function() {
          logEvent(NTP_LOGGING_EVENT_TYPE.NTP_FALLBACK_THUMBNAIL_USED);
          image.style.visibility = 'hidden';
          image2.style.visibility = 'visible';
        };
        logEvent(NTP_LOGGING_EVENT_TYPE.NTP_FALLBACK_THUMBNAIL_REQUESTED);
      } else {
        image.onerror = showDomainElement;
      }
      image.src = data.thumbnailUrl;
      logEvent(NTP_LOGGING_EVENT_TYPE.NTP_THUMBNAIL_ATTEMPT);
    } else {
      showDomainElement();
    }
  });
});
