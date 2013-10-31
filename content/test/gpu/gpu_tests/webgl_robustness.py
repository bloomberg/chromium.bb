# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry import test
from telemetry.page import page_set
from webgl_conformance import WebglConformanceValidator
from webgl_conformance import conformance_path


class WebglRobustness(test.Test):
  test = WebglConformanceValidator

  def CreatePageSet(self, options):
    page_set_dict = {
      'description': 'Test cases for WebGL robustness',
      'user_agent_type': 'desktop',
      'serving_dirs': [''],
      'pages': [
        {
          'url': 'file://extra/lots-of-polys-example.html',
          'inject_scripts': [
            os.path.join(os.path.dirname(__file__), 'webgl_conformance.js'),
            os.path.join(os.path.dirname(__file__), 'webgl_robustness.js')
          ],
          'navigate_steps': [
            { 'action': 'navigate' },
            { 'action': 'wait', 'javascript': 'webglTestHarness._finished' }
          ]
        }
      ]
    }
    return page_set.PageSet.FromDict(page_set_dict, conformance_path)
