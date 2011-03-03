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

    decorate: function() {
      gpu.Tab.prototype.decorate.apply(this);

      this.beginRequestClientInfo();

      this.logMessages_ = [];
      this.beginRequestLogMessages();

      browserBridge.addEventListener('gpuInfoUpdate', this.refresh.bind(this));
      this.refresh();
    },

    /**
     * This function begins a request for the ClientInfo. If it comes back
     * as undefined, then we will issue the request again in 250ms.
     */
    beginRequestClientInfo: function() {
      browserBridge.callAsync('requestClientInfo', undefined, (function(data) {
        this.clientInfo_ = data;
        this.refresh();
        if (data === undefined) { // try again in 250 ms
          window.setTimeout(this.beginRequestClientInfo.bind(this), 250);
        }
      }).bind(this));
    },

    /**
     * This function checks for new GPU_LOG messages.
     * If any are found, a refresh is triggered.
     */
    beginRequestLogMessages: function() {
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
      var diagnostics = this.querySelector('.diagnostics');
      var blacklistedIndicator = this.querySelector('.blacklisted-indicator');
      var gpuInfo = browserBridge.gpuInfo;
      if (gpuInfo) {
        if (gpuInfo.blacklistingReasons) {
          blacklistedIndicator.style.display = 'block';
          // Not using jstemplate here because we need to manipulate
          // href on the fly
          var reasonsEl = blacklistedIndicator.querySelector(
              '.blacklisted-reasons');
          reasonsEl.textContent = "";
          for (var i = 0; i < gpuInfo.blacklistingReasons.length; ++i) {
            var reason = gpuInfo.blacklistingReasons[i];

            var reasonEl = document.createElement('li');

            // Description of issue
            var desc = document.createElement('a');
            desc.textContent = reason.description;
            reasonEl.appendChild(desc);

            // Spacing ':' element
            if(reason.cr_bugs.length + reason.webkit_bugs.length > 0) {
              var tmp = document.createElement('span');
              tmp.textContent = '  ';
              reasonEl.appendChild(tmp);
            }

            var nreasons = 0;
            var j;
            // cr_bugs
            for (j =  0; j < reason.cr_bugs.length; ++j) {
              if (nreasons > 0) {
                var tmp = document.createElement('span');
                tmp.textContent = ', ';
                reasonEl.appendChild(tmp);
              }

              var lnk = document.createElement('a');
              var bugid = parseInt(reason.cr_bugs[j]);
              lnk.textContent = bugid;
              lnk.href = 'http://crbug.com/' + bugid;
              reasonEl.appendChild(lnk);
              nreasons += 1;
            }

            for (j =  0; j < reason.webkit_bugs.length; ++j) {
              if (nreasons > 0) {
                var tmp = document.createElement('span');
                tmp.textContent = ', ';
                reasonEl.appendChild(tmp);
              }

              var lnk = document.createElement('a');
              var bugid = parseInt(reason.webkit_bugs[j]);
              lnk.textContent = bugid;

              lnk.href = 'https://bugs.webkit.org/show_bug.cgi?id=' + bugid;
              reasonEl.appendChild(lnk);
              nreasons += 1;
            }

            reasonsEl.appendChild(reasonEl);
          }
        } else {
          blacklistedIndicator.style.display = 'none';
        }
        this.setTable_('basic-info', gpuInfo.basic_info);

        if (gpuInfo.diagnostics) {
          diagnostics.style.display = 'block';
          this.setTable_('diagnostics-table', gpuInfo.diagnostics);
        } else {
          diagnostics.style.display = 'none';
        }
      } else {
        blacklistedIndicator.style.display = 'none';
        this.setText_('basic-info', '... loading ...');
        diagnostics.style.display = 'none';
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