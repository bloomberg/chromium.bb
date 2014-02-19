// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

<include src="../../../../ui/webui/resources/js/util.js"></include>
<include src="viewport.js"></include>

// The plugin element is sized to fill the entire window and is set to be fixed
// positioning, acting as a viewport. The plugin renders into this viewport
// according to the scroll position of the window.
var plugin;

// This element is placed behind the plugin element to cause scrollbars to be
// displayed in the window. It is sized according to the document size of the
// pdf and zoom level.
var sizer;

// The toolbar element.
var viewerToolbar;

// The viewport object.
var viewport;

// Returns true if the fit-to-page button is enabled.
function isFitToPageEnabled() {
  return $('fit-to-page-button').classList.contains('polymer-selected');
}

// Called when a message is received from the plugin.
function handleMessage(message) {
  if (message.data.type == 'documentDimensions') {
    viewport.setDocumentDimensions(message.data);
  }
}

// Callback that's called when the viewport changes.
function viewportChangedCallback(zoom, x, y, scrollbarWidth, hasScrollbars) {
  // Offset the toolbar position so that it doesn't move if scrollbars appear.
  var toolbarRight = hasScrollbars.y ? 0 : scrollbarWidth;
  var toolbarBottom = hasScrollbars.x ? 0 : scrollbarWidth;
  viewerToolbar.style.right = toolbarRight + 'px';
  viewerToolbar.style.bottom = toolbarBottom + 'px';

  // Notify the plugin of the viewport change.
  plugin.postMessage({
    type: 'viewport',
    zoom: zoom,
    xOffset: x,
    yOffset: y
  });
}

function load() {
  sizer = $('sizer');
  viewerToolbar = $('toolbar');

  // Create the viewport.
  viewport = new Viewport(window,
                          sizer,
                          isFitToPageEnabled,
                          viewportChangedCallback);

  // Create the plugin object dynamically so we can set its src.
  plugin = document.createElement('object');
  plugin.id = 'plugin';
  plugin.type = 'application/x-google-chrome-pdf';
  plugin.addEventListener('message', handleMessage, false);
  // The pdf location is passed in the document url in the format:
  // http://<.../pdf.html>?<pdf location>.
  var url = window.location.search.substring(1);
  plugin.setAttribute('src', url);
  document.body.appendChild(plugin);

  // Setup the button event listeners.
  $('fit-to-width-button').addEventListener('click',
      viewport.fitToWidth.bind(viewport));
  $('fit-to-page-button').addEventListener('click',
      viewport.fitToPage.bind(viewport));
  $('zoom-in-button').addEventListener('click',
      viewport.zoomIn.bind(viewport));
  $('zoom-out-button').addEventListener('click',
      viewport.zoomOut.bind(viewport));

  // Setup keyboard event listeners.
  document.onkeydown = function(e) {
    switch (e.keyCode) {
      case 37:  // Left arrow key.
        // Go to the previous page if there are no horizontal scrollbars.
        if (!viewport.documentHasScrollbars().x) {
          viewport.goToPage(viewport.getMostVisiblePage() - 1);
          // Since we do the movement of the page
          e.preventDefault();
        }
        return;
      case 33:  // Page up key.
        // Go to the previous page if we are fit-to-page.
        if (isFitToPageEnabled()) {
          viewport.goToPage(viewport.getMostVisiblePage() - 1);
          e.preventDefault();
        }
        return;
      case 39:  // Right arrow key.
        // Go to the next page if there are no horizontal scrollbars.
        if (!viewport.documentHasScrollbars().x) {
          viewport.goToPage(viewport.getMostVisiblePage() + 1);
          e.preventDefault();
        }
        return;
      case 34:  // Page down key.
        // Go to the next page if we are fit-to-page.
        if (isFitToPageEnabled()) {
          viewport.goToPage(viewport.getMostVisiblePage() + 1);
          e.preventDefault();
        }
        return;
    }
  };
}

load();

})();
