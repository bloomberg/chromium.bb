# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import hardware_accelerated_feature_expectations as expectations

from telemetry import test
from telemetry.page import page_set
from telemetry.page import page_test

test_harness_script = r"""
  function VerifyHardwareAccelerated(feature) {
    feature += ': '
    var list = document.querySelector('.feature-status-list');
    for (var i=0; i < list.childElementCount; i++) {
      var span_list = list.children[i].getElementsByTagName('span');
      var feature_str = span_list[0].textContent;
      var value_str = span_list[1].textContent;
      if ((feature_str == feature) &&
          (value_str == 'Hardware accelerated')) {
        return true;
      }
    }
    return false;
  };
""";

class HardwareAcceleratedFeatureValidator(page_test.PageTest):
  def __init__(self):
    super(HardwareAcceleratedFeatureValidator, self).__init__('ValidatePage')

  def ValidatePage(self, page, tab, results):
    feature = page.feature
    if not tab.EvaluateJavaScript('VerifyHardwareAccelerated("%s")' % feature):
      raise page_test.Failure('%s not hardware accelerated' % feature)

def safe_feature_name(feature):
  return feature.lower().replace(' ', '_')

class HardwareAcceleratedFeature(test.Test):
  """Tests GPU acceleration is reported as active for various features"""
  test = HardwareAcceleratedFeatureValidator

  def CreateExpectations(self, page_set):
    return expectations.HardwareAcceleratedFeatureExpectations()

  def CreatePageSet(self, options):
    features = ['WebGL', 'Canvas', '3D CSS']

    page_set_dict = {
      'description': 'Tests GPU acceleration is reported as active',
      'user_agent_type': 'desktop',
      'pages': []
    }

    pages = page_set_dict['pages']

    for feature in features:
      pages.append({
        'name': 'HardwareAcceleratedFeature.%s_accelerated' %
            safe_feature_name(feature),
        'url': 'chrome://gpu',
        'navigate_steps': [
          { "action": 'navigate' }
        ],
        'script_to_evaluate_on_commit': test_harness_script,
        'feature': feature
      })

    return page_set.PageSet.FromDict(page_set_dict, '')