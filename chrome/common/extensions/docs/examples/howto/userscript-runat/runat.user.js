// ==UserScript==
// @name           Userscript @run-at example.
// @version        1.0.1
// @namespace      runat
// @description    This script demonstrates @runat by listing which resources
//                 the current page has loaded.
// @include        *
// @run-at         document-start
// ==/UserScript==

/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

var urls = {};
var count = 0;

/**
 * Called whenever the page loads a subresource.
 * Logs all loaded URLs in the urls var.
 */
document.addEventListener('beforeload', function(evt) {
  if (!urls[evt.url]) {
    urls[evt.url] = 0;
    count++;
  }
  urls[evt.url]++;
}, true);

/**
 * Called when the window is finished loading.
 * Loops through the urls var and prints its contents to the page DOM.
 */
window.addEventListener('load', function() {
  if (count == 0) {
    return;
  }

  // Create and style inserted DOM elements.
  var urls_dom = document.createElement('ul');
  urls_dom.style.cssText = [
      'margin: 4px 0 !important;',
      'padding: 0 !important;',
      'width: 98%;',
      'word-wrap: break-word;',
      'max-height: 200px;',
      'overflow: auto;'
  ].join(' ');
  var wrap_dom = document.createElement('div');
  wrap_dom.style.cssText = [
      'background-color: #ff7357;',
      'background-image: -webkit-repeating-linear-gradient(' +
          '-45deg, transparent, transparent 35px,' +
          'rgba(0,0,0,.1) 35px, rgba(0,0,0,.1) 70px);',
      'color: #000;',
      'padding: 10px;',
      'font: 14px Arial;'
  ].join(' ');
  var title_dom = document.createElement('strong');
  title_dom.textContent = (count > 1) ?
      'This page has loaded the following resources:' :
      'This page loaded the following resource:';
  wrap_dom.appendChild(title_dom);
  wrap_dom.appendChild(urls_dom);

  // Render each url as a list item containing a link.
  for (var url in urls) {
    var url_dom = document.createElement('li');
    var link_dom = document.createElement('a');
    var times_dom = document.createElement('span');
    url_dom.style.cssText = [
        'list-style-type: disc;',
        'margin: 0 0 0 30px !important;',
        'padding: 2px !important'
    ].join(' ');
    link_dom.setAttribute('href', url);
    link_dom.setAttribute('target', '_blank');
    link_dom.textContent = url;
    link_dom.style.cssText = [
        'color: #000 !important;'
    ].join(' ');
    times_dom.textContent = (urls[url] > 1) ?
        ' (' + urls[url] + ' times)' :
        ' (once)';
    url_dom.appendChild(link_dom);
    url_dom.appendChild(times_dom);
    urls_dom.appendChild(url_dom);
  }

  // Insert the created DOM into the page.
  document.body.style.position = 'relative';
  document.body.parentElement.insertBefore(wrap_dom, document.body);
}, true);
