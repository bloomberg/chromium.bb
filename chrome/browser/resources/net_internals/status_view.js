// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The status view at the top of the page.  It displays what mode net-internals
 * is in (capturing, viewing only, viewing loaded log), and may have a couple
 * buttons as well.
 */
var StatusView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * Main entry point. Called once the page has loaded.
   * @constructor
   */
  function StatusView() {
    assertFirstConstructorCall(StatusView);

    superClass.call(this, StatusView.MAIN_BOX_ID);

    $(StatusView.STOP_CAPTURING_BUTTON_ID).onclick = switchToViewOnlyMode_;

    $(StatusView.CLEAR_EVENTS_BUTTON_ID).onclick =
        g_browser.sourceTracker.deleteAllSourceEntries.bind(
            g_browser.sourceTracker);
    $(StatusView.CLEAR_CACHE_BUTTON_ID).onclick =
        g_browser.sendClearAllCache.bind(g_browser);
    $(StatusView.FLUSH_SOCKETS_BUTTON_ID).onclick =
        g_browser.sendFlushSocketPools.bind(g_browser);
  }

  // IDs for special HTML elements in status_view.html
  StatusView.MAIN_BOX_ID = 'status-view';
  StatusView.FOR_CAPTURE_ID = 'status-view-for-capture';
  StatusView.FOR_VIEW_ID = 'status-view-for-view';
  StatusView.FOR_FILE_ID = 'status-view-for-file';
  StatusView.STOP_CAPTURING_BUTTON_ID = 'status-view-stop-capturing';
  StatusView.CLEAR_EVENTS_BUTTON_ID = 'status-view-clear-events';
  StatusView.CLEAR_CACHE_BUTTON_ID = 'status-view-clear-cache';
  StatusView.FLUSH_SOCKETS_BUTTON_ID = 'status-view-flush-sockets';
  StatusView.DUMP_FILE_NAME_ID = 'status-view-dump-file-name';

  cr.addSingletonGetter(StatusView);

  StatusView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    /**
     * Called when switching mode.  Hides all the StatusView nodes except for
     * |id|, and updates the file name displayed when viewing a file.  When
     * not viewing a file, |fileName| is ignored.
     */
    onSwitchMode: function(id, fileName) {
      setNodeDisplay($(StatusView.FOR_CAPTURE_ID),
                     StatusView.FOR_CAPTURE_ID == id);
      setNodeDisplay($(StatusView.FOR_VIEW_ID),
                     StatusView.FOR_VIEW_ID == id);
      setNodeDisplay($(StatusView.FOR_FILE_ID),
                     StatusView.FOR_FILE_ID == id);

      if (StatusView.FOR_FILE_ID == id)
        $(StatusView.DUMP_FILE_NAME_ID).innerText = fileName;
    }
  };

  /**
   * Calls the corresponding function of MainView.  This is needed because we
   * can't call MainView.getInstance() in the constructor, as it hasn't been
   * created yet.
   */
  function switchToViewOnlyMode_() {
    MainView.getInstance().switchToViewOnlyMode();
  }

  return StatusView;
})();
