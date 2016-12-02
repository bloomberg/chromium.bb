# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from gpu_tests import gpu_test_base
import gpu_tests.hardware_accelerated_feature_expectations as hw_expectations

from telemetry.page import legacy_page_test
from telemetry.story import story_set as story_set_module

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
"""

class HardwareAcceleratedFeatureValidator(gpu_test_base.ValidatorBase):
  def ValidateAndMeasurePage(self, page, tab, results):
    feature = page.feature
    if not tab.EvaluateJavaScript('VerifyHardwareAccelerated("%s")' % feature):
      print 'Test failed. Printing page contents:'
      print tab.EvaluateJavaScript('document.body.innerHTML')
      raise legacy_page_test.Failure('%s not hardware accelerated' % feature)

def safe_feature_name(feature):
  return feature.lower().replace(' ', '_')

class ChromeGpuPage(gpu_test_base.PageBase):
  def __init__(self, story_set, feature, expectations):
    super(ChromeGpuPage, self).__init__(
      url='chrome://gpu', page_set=story_set, base_dir=story_set.base_dir,
      name=('HardwareAcceleratedFeature.%s_accelerated' %
            safe_feature_name(feature)),
      expectations=expectations)
    self.feature = feature
    self.script_to_evaluate_on_commit = test_harness_script

class HardwareAcceleratedFeature(gpu_test_base.TestBase):
  """Tests GPU acceleration is reported as active for various features"""
  test = HardwareAcceleratedFeatureValidator

  @classmethod
  def Name(cls):
    return 'hardware_accelerated_feature'

  def _CreateExpectations(self):
    return hw_expectations.HardwareAcceleratedFeatureExpectations()

  def CreateStorySet(self, options):
    features = ['WebGL', 'Canvas']

    ps = story_set_module.StorySet()

    for feature in features:
      ps.AddStory(ChromeGpuPage(story_set=ps, feature=feature,
                                expectations=self.GetExpectations()))
    return ps
