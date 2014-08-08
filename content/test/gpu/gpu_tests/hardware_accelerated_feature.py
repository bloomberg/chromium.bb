# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import hardware_accelerated_feature_expectations as expectations

from telemetry import benchmark
from telemetry.page import page as page_module
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

class _HardwareAcceleratedFeatureValidator(page_test.PageTest):
  def ValidateAndMeasurePage(self, page, tab, results):
    feature = page.feature
    if not tab.EvaluateJavaScript('VerifyHardwareAccelerated("%s")' % feature):
      print 'Test failed. Printing page contents:'
      print tab.EvaluateJavaScript('document.body.innerHTML')
      raise page_test.Failure('%s not hardware accelerated' % feature)

def safe_feature_name(feature):
  return feature.lower().replace(' ', '_')

class ChromeGpuPage(page_module.Page):
  def __init__(self, page_set, feature):
    super(ChromeGpuPage, self).__init__(
      url='chrome://gpu', page_set=page_set, base_dir=page_set.base_dir,
      name=('HardwareAcceleratedFeature.%s_accelerated' %
            safe_feature_name(feature)))
    self.feature = feature
    self.script_to_evaluate_on_commit = test_harness_script

class HardwareAcceleratedFeature(benchmark.Benchmark):
  """Tests GPU acceleration is reported as active for various features"""
  test = _HardwareAcceleratedFeatureValidator

  def CreateExpectations(self, page_set):
    return expectations.HardwareAcceleratedFeatureExpectations()

  def CreatePageSet(self, options):
    features = ['WebGL', 'Canvas']

    ps = page_set.PageSet(user_agent_type='desktop', file_path='')

    for feature in features:
      ps.AddPage(ChromeGpuPage(page_set=ps, feature=feature))
    return ps
