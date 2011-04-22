// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview This view displays information on the current GPU
 * hardware.  Its primary usefulness is to allow users to copy-paste
 * their data in an easy to read format for bug reports.
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

      browserBridge.addEventListener('gpuInfoUpdate', this.refresh.bind(this));
      browserBridge.addEventListener('logMessagesChange',
                                     this.refresh.bind(this));
      browserBridge.addEventListener('clientInfoChange',
                                     this.refresh.bind(this));
      this.refresh();
    },

    /**
    * Updates the view based on its currently known data
    */
    refresh: function(data) {
      // Client info
      if (browserBridge.clientInfo) {
        var clientInfo = browserBridge.clientInfo;
        var chromeVersion = clientInfo.version +
            ' (' + clientInfo.official +
            ' ' + clientInfo.cl +
            ') ' + clientInfo.version_mod;
        this.setTable_('client-info', [
          {
            description: 'Data exported',
            value: (new Date()).toLocaleString()
          },
          {
            description: 'Chrome version',
            value: chromeVersion
          },
          {
            description: 'Software rendering list version',
            value: clientInfo.blacklist_version
          }]);
      } else {
        this.setText_('client-info', '... loading...');
      }

      // Feature map
      var featureLabelMap = {
        '2d_canvas': 'Canvas',
        '3d_css': '3D CSS',
        'compositing': 'Compositing',
        'webgl': 'WebGL',
        'multisampling': 'WebGL multisampling'
      };
      var statusLabelMap = {
        'disabled_software': 'Software only. Hardware acceleration disabled.',
        'disabled_off': 'Unavailable. Hardware acceleration disabled.',
        'software': 'Software rendered. Hardware acceleration not enabled.',
        'unavailable_off': 'Unavailable. Hardware acceleration unavailable',
        'unavailable_software':
            'Software only, hardware acceleration unavailable',
        'enabled': 'Hardware accelerated'
      };
      var statusClassMap = {
        'disabled_software': 'feature-yellow',
        'disabled_off': 'feature-red',
        'software': 'feature-yellow',
        'unavailable_off': 'feature-red',
        'unavailable_software': 'feature-yellow',
        'enabled': 'feature-green'
      };

      // GPU info, basic
      var diagnosticsDiv = this.querySelector('.diagnostics');
      var diagnosticsLoadingDiv = this.querySelector('.diagnostics-loading');
      var featureStatusList = this.querySelector('.feature-status-list');
      var problemsDiv = this.querySelector('.problems-div');
      var problemsList = this.querySelector('.problems-list');
      var gpuInfo = browserBridge.gpuInfo;
      var i;
      if (gpuInfo) {
        // Not using jstemplate here for blacklist status because we construct
        // href from data, which jstemplate can't seem to do.
        if (gpuInfo.featureStatus) {
          // feature status list
          featureStatusList.textContent = '';
          for (i = 0; i < gpuInfo.featureStatus.featureStatus.length;
               i++) {
            var feature = gpuInfo.featureStatus.featureStatus[i];
            var featureEl = document.createElement('li');

            var nameEl = document.createElement('span');
            if (!featureLabelMap[feature.name])
              console.log('Missing featureLabel for', feature.name);
            nameEl.textContent = featureLabelMap[feature.name] + ': ';
            featureEl.appendChild(nameEl);

            var statusEl = document.createElement('span');
            if (!statusLabelMap[feature.status])
              console.log('Missing statusLabel for', feature.status);
            if (!statusClassMap[feature.status])
              console.log('Missing statusClass for', feature.status);
            statusEl.textContent = statusLabelMap[feature.status];
            statusEl.className = statusClassMap[feature.status];
            featureEl.appendChild(statusEl);

            featureStatusList.appendChild(featureEl);
          }

          // problems list
          if (gpuInfo.featureStatus.problems.length) {
            problemsDiv.hidden = false;
            problemsList.textContent = '';
            for (i = 0; i < gpuInfo.featureStatus.problems.length; i++) {
              var problem = gpuInfo.featureStatus.problems[i];
              var problemEl = this.createProblemEl_(problem);
              problemsList.appendChild(problemEl);
            }
          } else {
            problemsDiv.hidden = true;
          }

        } else {
          featureStatusList.textContent = '';
          problemsList.hidden = true;
        }
        if (gpuInfo.basic_info)
          this.setTable_('basic-info', gpuInfo.basic_info);
        else
          this.setTable_('basic-info', []);

        if (gpuInfo.diagnostics) {
          diagnosticsDiv.hidden = false;
          diagnosticsLoadingDiv.hidden = true;
          $('diagnostics-table').hidden = false;
          this.setTable_('diagnostics-table', gpuInfo.diagnostics);
          this.querySelector('diagnostics-status').hidden = true;
        } else if (gpuInfo.diagnostics === null) {
          // gpu_internals.cc sets diagnostics to null when it is being loaded
          diagnosticsDiv.hidden = false;
          diagnosticsLoadingDiv.hidden = false;
          $('diagnostics-table').hidden = true;
        } else {
          diagnosticsDiv.hidden = true;
        }
      } else {
        this.setText_('basic-info', '... loading ...');
        diagnosticsDiv.hidden = true;
        featureStatusList.textContent = '';
        problemsDiv.hidden = true;
      }

      // Log messages
      jstProcess(new JsEvalContext({values: browserBridge.logMessages}),
                 document.getElementById('log-messages'));
    },

    createProblemEl_: function(problem) {
      var problemEl;
      problemEl = document.createElement('li');

      // Description of issue
      var desc = document.createElement('a');
      desc.textContent = problem.description;
      problemEl.appendChild(desc);

      // Spacing ':' element
      if (problem.crBugs.length + problem.webkitBugs.length > 0) {
        var tmp = document.createElement('span');
        tmp.textContent = ': ';
        problemEl.appendChild(tmp);
      }

      var nbugs = 0;
      var j;

      // crBugs
      for (j = 0; j < problem.crBugs.length; ++j) {
        if (nbugs > 0) {
          var tmp = document.createElement('span');
          tmp.textContent = ', ';
          problemEl.appendChild(tmp);
        }

        var link = document.createElement('a');
        var bugid = parseInt(problem.crBugs[j]);
        link.textContent = bugid;
        link.href = 'http://crbug.com/' + bugid;
        problemEl.appendChild(link);
        nbugs++;
      }

      for (j = 0; j < problem.webkitBugs.length; ++j) {
        if (nbugs > 0) {
          var tmp = document.createElement('span');
          tmp.textContent = ', ';
          problemEl.appendChild(tmp);
        }

        var link = document.createElement('a');
        var bugid = parseInt(problem.webkitBugs[j]);
        link.textContent = bugid;

        link.href = 'https://bugs.webkit.org/show_bug.cgi?id=' + bugid;
        problemEl.appendChild(link);
        nbugs++;
      }

      return problemEl;
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
