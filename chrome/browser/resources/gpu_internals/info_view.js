// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
   * @extends {cr.ui.TabPanel}
   */
  var InfoView = cr.ui.define(cr.ui.TabPanel);

  InfoView.prototype = {
    __proto__: cr.ui.TabPanel.prototype,

    decorate: function() {
      cr.ui.TabPanel.prototype.decorate.apply(this);

      browserBridge.addEventListener('gpuInfoUpdate', this.refresh.bind(this));
      browserBridge.addEventListener('logMessagesChange',
                                     this.refresh.bind(this));
      browserBridge.addEventListener('clientInfoChange',
                                     this.refresh.bind(this));
      browserBridge.addEventListener('crashListChange',
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
            description: 'Operating system',
            value: clientInfo.operating_system
          },
          {
            description: 'Software rendering list version',
            value: clientInfo.blacklist_version
          },
          {
            description: 'ANGLE revision',
            value: clientInfo.angle_revision
          },
          {
            description: '2D graphics backend',
            value: clientInfo.graphics_backend
          }]);
      } else {
        this.setText_('client-info', '... loading...');
      }

      // Feature map
      var featureLabelMap = {
        '2d_canvas': 'Canvas',
        '3d_css': '3D CSS',
        'css_animation': 'CSS Animation',
        'compositing': 'Compositing',
        'webgl': 'WebGL',
        'multisampling': 'WebGL multisampling',
        'flash_3d': 'Flash 3D',
        'flash_stage3d': 'Flash Stage3D',
        'texture_sharing': 'Texture Sharing',
        'video_decode': 'Video Decode',
        // GPU Switching
        'gpu_switching': 'GPU Switching',
      };
      var statusLabelMap = {
        'disabled_software': 'Software only. Hardware acceleration disabled.',
        'disabled_software_animated': 'Software animated.',
        'disabled_off': 'Unavailable. Hardware acceleration disabled.',
        'software': 'Software rendered. Hardware acceleration not enabled.',
        'unavailable_off': 'Unavailable. Hardware acceleration unavailable',
        'unavailable_software':
            'Software only, hardware acceleration unavailable',
        'enabled_readback': 'Hardware accelerated, but at reduced performance',
        'enabled_force': 'Hardware accelerated on all pages',
        'enabled_threaded': 'Hardware accelerated on demand and threaded',
        'enabled_force_threaded':
            'Hardware accelerated on all pages and threaded',
        'enabled': 'Hardware accelerated',
        'accelerated': 'Accelerated',
        'accelerated_threaded': 'Accelerated and threaded',
        // GPU Switching
        'gpu_switching_automatic': 'Automatic switching',
        'gpu_switching_force_discrete': 'Always on discrete GPU',
        'gpu_switching_force_integrated': 'Always on integrated GPU',
      };

      var statusClassMap = {
        'disabled_software': 'feature-yellow',
        'disabled_software_animated': 'feature-yellow',
        'disabled_off': 'feature-red',
        'software': 'feature-yellow',
        'unavailable_off': 'feature-red',
        'unavailable_software': 'feature-yellow',
        'enabled_force': 'feature-green',
        'enabled_readback': 'feature-yellow',
        'enabled_threaded': 'feature-green',
        'enabled_force_threaded': 'feature-green',
        'enabled': 'feature-green',
        'accelerated': 'feature-green',
        'accelerated_threaded': 'feature-green',
        // GPU Switching
        'gpu_switching_automatic': 'feature-green',
        'gpu_switching_force_discrete': 'feature-red',
        'gpu_switching_force_integrated': 'feature-red',
      };

      // GPU info, basic
      var diagnosticsDiv = this.querySelector('.diagnostics');
      var diagnosticsLoadingDiv = this.querySelector('.diagnostics-loading');
      var featureStatusList = this.querySelector('.feature-status-list');
      var problemsDiv = this.querySelector('.problems-div');
      var problemsList = this.querySelector('.problems-list');
      var performanceDiv = this.querySelector('.performance-div');
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

        if (gpuInfo.performance_info) {
          performanceDiv.hidden = false;
          this.setTable_('performance-info', gpuInfo.performance_info);
        } else {
          performanceDiv.hidden = true;
        }

        if (gpuInfo.diagnostics) {
          diagnosticsDiv.hidden = false;
          diagnosticsLoadingDiv.hidden = true;
          $('diagnostics-table').hidden = false;
          this.setTable_('diagnostics-table', gpuInfo.diagnostics);
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

      // Crash list
      jstProcess(new JsEvalContext({values: browserBridge.crashList}),
                 $('crash-list'));

      // Log messages
      jstProcess(new JsEvalContext({values: browserBridge.logMessages}),
                 $('log-messages'));
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
      peg.textContent = text;
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
