// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays a summary of the state of each HTTP pipelined connection,
 * and has links to display them in the events tab.
 */
var HttpPipelineView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function HttpPipelineView() {
    assertFirstConstructorCall(HttpPipelineView);

    // Call superclass's constructor.
    superClass.call(this, HttpPipelineView.MAIN_BOX_ID);

    g_browser.addHttpPipeliningStatusObserver(this, true);
  }

  HttpPipelineView.TAB_ID = 'tab-handle-http-pipeline';
  HttpPipelineView.TAB_NAME = 'Pipelining';
  HttpPipelineView.TAB_HASH = '#httpPipeline';

  // IDs for special HTML elements in http_pipeline_view.html
  HttpPipelineView.MAIN_BOX_ID = 'http-pipeline-view-tab-content';
  HttpPipelineView.ENABLED_SPAN_ID = 'http-pipeline-view-enabled-span';
  HttpPipelineView.KNOWN_HOSTS_TABLE_ID =
      'http-pipeline-view-known-hosts-table';

  cr.addSingletonGetter(HttpPipelineView);

  HttpPipelineView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogFinish: function(data) {
      return this.onHttpPipeliningStatusChanged(data.httpPipeliningStatus);
    },

    /**
     * Displays information on the global HTTP pipelining status.
     */
    onHttpPipeliningStatusChanged: function(httpPipeliningStatus) {
      if (!httpPipeliningStatus)
        return false;
      var input = new JsEvalContext(httpPipeliningStatus);
      jstProcess(input, $(HttpPipelineView.MAIN_BOX_ID));
      // Hide view in loaded logs if pipelining isn't enabled.
      return httpPipeliningStatus.pipelining_enabled;
    },
  };

  return HttpPipelineView;
})();
