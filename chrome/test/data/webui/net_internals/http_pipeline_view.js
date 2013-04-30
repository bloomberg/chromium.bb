// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['net_internals_test.js']);

// Anonymous namespace
(function() {

/**
 * Checks the display on the HTTP Pipelining tab against the information it
 * should be displaying.
 * @param {object} httpPipelineStatus Results from a http pipeline status query.
 */
function checkDisplay(httpPipelineStatus) {
  expectEquals(httpPipelineStatus.pipelining_enabled,
               $(HttpPipelineView.ENABLED_SPAN_ID).innerText == "true");
}

/**
 * Finds an entry with the specified host name and port in the
 * |pipelinedHostInfo| cache, and returns its index.
 * @param {object} pipelinedHostInfo Results to search.
 * @param {string} hostname The host name of the host to find.
 * @param {int} port The port of the host to find.
 * @return {int} Index of the specified host.  -1 if not found.
 */
function findEntry(pipelinedHostInfo, hostname, port) {
  var expected = hostname + ":" + port;
  for (var i = 0; i < pipelinedHostInfo.length; ++i) {
    if (pipelinedHostInfo[i].host == expected)
      return i;
  }
  return -1;
}

/**
 * A Task that adds a pipeline capability to the known hosts map and waits for
 * it to appear in the data we receive from the browser.
 * @param {string} hostname Name of host address we're waiting for.
 * @param {int} port Port of the host we're waiting for.
 * @param {string} capability The capability to set.
 * @extends {NetInternalsTest.Task}
 */
function AddPipelineCapabilityTask(hostname, port, capability) {
  this.hostname_ = hostname;
  this.port_ = port;
  this.capability_ = capability;
  NetInternalsTest.Task.call(this);
}

AddPipelineCapabilityTask.prototype = {
  __proto__: NetInternalsTest.Task.prototype,

  /**
   * Adds an entry to the host map and starts waiting to receive the results
   * from the browser process.
   */
  start: function() {
    var addPipelineCapabilityParams = [
      this.hostname_,
      this.port_,
      this.capability_
    ];
    chrome.send('addDummyHttpPipelineFeedback', addPipelineCapabilityParams);
    g_browser.addHttpPipeliningStatusObserver(this, false);
  },

  /**
   * Callback from the BrowserBridge.  Checks if |httpPipelineStatus| has the
   * known host specified on creation.  If so, validates it and completes the
   * task.  If not, continues running.
   * @param {object} httpPipelineStatus Result of a http pipeline status query.
   */
  onHttpPipeliningStatusChanged: function(httpPipelineStatus) {
    if (!this.isDone()) {
      checkDisplay(httpPipelineStatus);

      var index = findEntry(httpPipelineStatus.pipelined_host_info,
                            this.hostname_, this.port_);
      if (index >= 0) {
        var entry = httpPipelineStatus.pipelined_host_info[index];
        expectEquals(this.capability_, entry.capability);

        var hostPortText = NetInternalsTest.getTbodyText(
            HttpPipelineView.KNOWN_HOSTS_TABLE_ID, index, 0);
        expectEquals(this.hostname_ + ":" + this.port_, hostPortText);
        var capabilityText = NetInternalsTest.getTbodyText(
            HttpPipelineView.KNOWN_HOSTS_TABLE_ID, index, 1);
        expectEquals(this.capability_, capabilityText);

        this.onTaskDone();
      }
    }
  }
};

/**
 * Adds a capable pipelining host.
 */
TEST_F('NetInternalsTest', 'netInternalsHttpPipelineViewCapable', function() {
  // Since this is called before we switch to the HTTP Pipelining view, we'll
  // never see the original pipelining state.
  chrome.send('enableHttpPipelining', [true]);

  NetInternalsTest.switchToView('httpPipeline');
  var taskQueue = new NetInternalsTest.TaskQueue(true);
  taskQueue.addTask(new AddPipelineCapabilityTask(
      'somewhere.com', 80, 'capable'));
  taskQueue.run(true);
});

/**
 * Adds an incapable pipelining host.
 */
TEST_F('NetInternalsTest', 'netInternalsHttpPipelineViewIncapable', function() {
  // Since this is called before we switch to the HTTP Pipelining view, we'll
  // never see the original pipelining state.
  chrome.send('enableHttpPipelining', [true]);

  NetInternalsTest.switchToView('httpPipeline');
  var taskQueue = new NetInternalsTest.TaskQueue(true);
  taskQueue.addTask(new AddPipelineCapabilityTask(
      'elsewhere.com', 1234, 'incapable'));
  taskQueue.run(true);
});

/**
 * Checks with pipelining disabled.
 * TODO(mmenke):  Make this test wait to receive pipelining state.  Currently
 *                just checks the default state, before data is received.
 */
TEST_F('NetInternalsTest', 'netInternalsHttpPipelineViewDisabled', function() {
  NetInternalsTest.switchToView('httpPipeline');
  var expected_status = { pipelining_enabled: false }
  checkDisplay(expected_status);
  testDone();
});

})();  // Anonymous namespace
