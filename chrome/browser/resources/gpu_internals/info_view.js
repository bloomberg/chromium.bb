// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays options for importing/exporting the captured data. Its
 * primarily usefulness is to allow users to copy-paste their data in an easy
 * to read format for bug reports.
 *
 *   - Has a button to generate a text report.
 *
 *   - Shows how many events have been captured.
 */
cr.define('gpu', function() {
  /**
   * Provides information on the GPU process and underlying graphics hardware.
   * @constructor
   * @extends {Tab}
   */
  var InfoView = cr.ui.define(gpu.Tab);

  InfoView.prototype = {
    __proto__: gpu.Tab.prototype,

    decorate : function() {
      gpu.Tab.prototype.decorate.apply(this);

      this.beginRequestClientInfo();
      this.beginRequestGpuInfo();

      this.logMessages_ = [];
      this.beginRequestLogMessages();

      this.refresh();
    },

    /**
     * This function begins a request for the ClientInfo. If it comes back
     * as undefined, then we will issue the request again in 250ms.
     */
    beginRequestClientInfo : function() {
      browserBridge.callAsync('requestClientInfo', undefined, (function(data) {
        this.clientInfo_ = data;
        this.refresh();
        if (data === undefined) { // try again in 250 ms
          window.setTimeout(this.beginRequestClientInfo.bind(this), 250);
        }
      }).bind(this));
    },

    /**
     * This function begins a request for the GpuInfo. If it comes back
     * as undefined, then we will issue the request again in 250ms.
     */
    beginRequestGpuInfo : function() {
      browserBridge.callAsync('requestGpuInfo', undefined, (function(data) {
        this.gpuInfo_ = data;
        this.refresh();
        if (!data || data.progress != 'complete') { // try again in 250 ms
          window.setTimeout(this.beginRequestGpuInfo.bind(this), 250);
        }
      }).bind(this));
    },

    /**
     * This function checks for new GPU_LOG messages.
     * If any are found, a refresh is triggered.
     */
    beginRequestLogMessages : function() {
      browserBridge.callAsync('requestLogMessages', undefined,
        (function(messages) {
           if(messages.length != this.logMessages_.length) {
             this.logMessages_ = messages;
             this.refresh();
           }
           // check again in 250 ms
           window.setTimeout(this.beginRequestLogMessages.bind(this), 250);
         }).bind(this));
    },


    /**
    * Updates the view based on its currently known data
    */
    refresh: function(data) {
      // Client info
      if (this.clientInfo_) {
        var chromeVersion = this.clientInfo_.version +
          ' (' + this.clientInfo_.official +
          ' ' + this.clientInfo_.cl +
          ') ' + this.clientInfo_.version_mod;
        this.setTable_('client-info', [
            {
              description: 'Data exported',
              value: (new Date()).toLocaleString()
            },
            {
              description: 'Chrome version',
              value: chromeVersion
            }]);
      } else {
        this.setText_('client-info', '... loading...');
      }

      // GPU info, basic
      if (this.gpuInfo_) {
        this.setTable_('basic-info', this.gpuInfo_.basic_info);
        if (this.gpuInfo_.diagnostics) {
          this.setTable_('diagnostics', this.gpuInfo_.diagnostics);
        } else if (this.gpuInfo_.progress == 'partial') {
          this.setText_('diagnostics', '... loading...');
        } else {
          this.setText_('diagnostics', 'None');
        }
      } else {
        this.setText_('basic-info', '... loading ...');
        this.setText_('diagnostics', '... loading ...');
      }

      // Log messages
      jstProcess(new JsEvalContext({values: this.logMessages_}),
                 document.getElementById('log-messages'));
    },

    setText_: function(outputElementId, text) {
      var peg = document.getElementById(outputElementId);
      peg.innerText = text;
    },

    setTable_: function(outputElementId, inputData) {
      var template = jstGetTemplate('info-view-table-template');
      jstProcess(new JsEvalContext({value: inputData}),
                 template);

      var peg = document.getElementById(outputElementId);
      if (!peg)
        throw new Error('Node ' + outputElementId + ' not found');

      peg.innerHTML = '';
      peg.appendChild(template);
    }
  };

  return {
    InfoView: InfoView
  };
});