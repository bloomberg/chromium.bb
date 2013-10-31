# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import optparse
import os

from telemetry import test as test_module
from telemetry.core import util
from telemetry.page import page_set
from telemetry.page import page_test

data_path = os.path.join(
    util.GetChromiumSrcDir(), 'content', 'test', 'data', 'gpu')


class _WebGLDebugRendererInfoValidator(page_test.PageTest):
  def __init__(self):
    super(_WebGLDebugRendererInfoValidator, self).__init__('ValidatePage')

  def CustomizeBrowserOptions(self, options):
    chrome_finch_option = options.finch_option
    if options.finch_option == "bypassed":
      chrome_finch_option = "enabled"
      options.AppendExtraBrowserArgs('--disable-webgl-debug-renderer-info')
    options.AppendExtraBrowserArgs(
        '--force-fieldtrials=WebGLDebugRendererInfo/%s/' %
            chrome_finch_option)
    options.AppendExtraBrowserArgs('--google-base-url=http://127.0.0.1')

  def InjectJavascript(self):
    return [os.path.join(os.path.dirname(__file__),
            'webgl_debug_renderer_info.js')]

  def ValidatePage(self, page, tab, results):
    if not tab.EvaluateJavaScript('window.domAutomationController._succeeded'):
      raise page_test.Failure('Test failed')

class WebGLDebugRendererInfo(test_module.Test):
  test = _WebGLDebugRendererInfoValidator

  @staticmethod
  def AddTestCommandLineOptions(parser):
    group = optparse.OptionGroup(parser, 'Finch experiment options')
    group.add_option('--finch-option',
        help = """
Options: enabled, disabled, bypassed
  enabled  = experiment forced on, expect extension to be present
  disabled = experiment forced off, expect extension to be absent
  bypassed = experiment forced on, disabled via command line flag,
             expect extension to be absent
""",
        default='enabled')
    parser.add_option_group(group)

  def CreatePageSet(self, options):
    page_set_dict = {
      'description': 'Test cases for WEBGL_debug_renderer_info extension',
      'user_agent_type': 'desktop',
      'serving_dirs': [''],
      'pages': [
        {
          'name': 'WEBGL_debug_render_info.%s' %
              options.finch_option,
          'url': 'file://webgl_debug_renderer_info.html?query=%s' %
              options.finch_option,
          'navigate_steps': [
            { 'action': 'navigate' }
          ]
        }
      ]
    }
    return page_set.PageSet.FromDict(page_set_dict, data_path)
