// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information related to Prerendering.
 */
var PrerenderView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function PrerenderView() {
    assertFirstConstructorCall(PrerenderView);

    // Call superclass's constructor.
    superClass.call(this, PrerenderView.MAIN_BOX_ID);

    g_browser.addPrerenderInfoObserver(this);
    this.prerenderEnabledSpan_ = $(PrerenderView.ENABLED_SPAN_ID);
    this.prerenderOmniboxEnabledSpan_ =
        $(PrerenderView.OMNIBOX_ENABLED_SPAN_ID);
    this.prerenderHistoryDiv_ = $(PrerenderView.HISTORY_DIV_ID);
    this.prerenderActiveDiv_ = $(PrerenderView.ACTIVE_DIV_ID);
  }

  // ID for special HTML element in category_tabs.html
  PrerenderView.TAB_HANDLE_ID = 'tab-handle-prerender';

  // IDs for special HTML elements in prerender_view.html
  PrerenderView.MAIN_BOX_ID = 'prerender-view-tab-content';
  PrerenderView.ENABLED_SPAN_ID = 'prerender-view-enabled-span';
  PrerenderView.OMNIBOX_ENABLED_SPAN_ID = 'prerender-view-omnibox-enabled-span';
  PrerenderView.HISTORY_DIV_ID = 'prerender-view-history-div';
  PrerenderView.ACTIVE_DIV_ID = 'prerender-view-active-div';

  cr.addSingletonGetter(PrerenderView);

  PrerenderView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogFinish: function(data) {
      return this.onPrerenderInfoChanged(data.prerenderInfo);
    },

    onPrerenderInfoChanged: function(prerenderInfo) {
      this.prerenderEnabledSpan_.textContent = '';
      this.prerenderOmniboxEnabledSpan_.textContent = '';
      this.prerenderHistoryDiv_.innerHTML = '';
      this.prerenderActiveDiv_.innerHTML = '';

      if (prerenderInfo && ('enabled' in prerenderInfo)) {
        this.prerenderEnabledSpan_.textContent =
            prerenderInfo.enabled.toString();
        if (prerenderInfo.disabled_reason) {
          this.prerenderEnabledSpan_.textContent +=
              ' ' + prerenderInfo.disabled_reason;
        }
      }

      if (prerenderInfo && ('omnibox_enabled' in prerenderInfo)) {
        this.prerenderOmniboxEnabledSpan_.textContent =
            prerenderInfo.omnibox_enabled.toString();
      }

      if (!isValidPrerenderInfo(prerenderInfo))
        return false;

      var tabPrinter = createHistoryTablePrinter(prerenderInfo.history);
      tabPrinter.toHTML(this.prerenderHistoryDiv_, 'styledTable');

      var tabPrinter = createActiveTablePrinter(prerenderInfo.active);
      tabPrinter.toHTML(this.prerenderActiveDiv_, 'styledTable');

      return true;
    }
  };

  function isValidPrerenderInfo(prerenderInfo) {
    if (prerenderInfo == null) {
      return false;
    }
    if (!('history' in prerenderInfo) ||
        !('active' in prerenderInfo) ||
        !('enabled' in prerenderInfo)) {
      return false;
    }
    return true;
  }

  function createHistoryTablePrinter(prerenderHistory) {
    var tablePrinter = new TablePrinter();
    tablePrinter.addHeaderCell('Origin');
    tablePrinter.addHeaderCell('URL');
    tablePrinter.addHeaderCell('Final Status');
    tablePrinter.addHeaderCell('Time');

    for (var i = 0; i < prerenderHistory.length; i++) {
      var historyEntry = prerenderHistory[i];
      tablePrinter.addRow();
      tablePrinter.addCell(historyEntry.origin);
      tablePrinter.addCell(historyEntry.url);
      tablePrinter.addCell(historyEntry.final_status);

      var date = new Date(parseInt(historyEntry.end_time));
      tablePrinter.addCell(date.toLocaleString());
    }
    return tablePrinter;
  }

  function createActiveTablePrinter(prerenderActive) {
    var tablePrinter = new TablePrinter();
    tablePrinter.addHeaderCell('URL');
    tablePrinter.addHeaderCell('Duration');

    for (var i = 0; i < prerenderActive.length; i++) {
      var activeEntry = prerenderActive[i];
      tablePrinter.addRow();
      tablePrinter.addCell(activeEntry.url);
      tablePrinter.addCell(activeEntry.duration);
    }
    return tablePrinter;
  }

  return PrerenderView;
})();
