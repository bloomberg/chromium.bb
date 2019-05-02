# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
from gpu_tests import test_expectations

import inspect
import re

ANGLE_CONDITIONS = ['d3d9', 'd3d11', 'opengl', 'opengles', 'vulkan', 'no_angle']
CMD_DECODER_CONDITIONS = ['passthrough', 'no_passthrough']
GPU_CONDITIONS = ['amd', 'arm', 'broadcom', 'hisilicon', 'intel', 'imagination',
                  'nvidia', 'qualcomm', 'vivante']

class GpuExpectation(test_expectations.Expectation):
  def __init__(self, expectation, pattern, conditions=None, bug=None,
               max_num_retries=0):
    self.gpu_conditions = []
    self.device_id_conditions = []
    self.angle_conditions = []
    self.cmd_decoder_conditions = []
    self.max_num_retries = max_num_retries
    assert self.max_num_retries == 0 or expectation == 'flaky'
    super(GpuExpectation, self).__init__(
      expectation, pattern, conditions=conditions, bug=bug)

  def ParseCondition(self, condition):
    """Adds support for GPU, device ID, and ANGLE conditions.

    GPU vendors:
      amd, arm, broadcom, hisilicon, intel, imagination, nvidia,
      qualcomm, vivante

    ANGLE renderer:
      d3d9, d3d11, opengl, no_angle
      no_angle can be used to avoid conflicts between expectations for
      ANGLE and expectations not for ANGLE

    Specific GPUs can be listed as a tuple with vendor name and device ID.
    Examples: ('nvidia', 0x1234), ('arm', 'Mali-T604')
    Device IDs must be paired with a GPU vendor.

    Sample usage in SetExpectations in subclasses:
      self.Fail('gl-enable-vertex-attrib.html',
          ['mac', 'amd', ('nvidia', 0x1234)], bug=123)
    """
    if isinstance(condition, tuple):
      c0 = condition[0].lower()
      if c0 in GPU_CONDITIONS:
        device = condition[1]
        if isinstance(device, str):
          # If the device is parseable as an int, that's not allowed.
          # It's too easy to make a mistake specifying the device ID
          # as a string instead of an int.
          was_int = False
          try:
            int(device, 0)
            was_int = True
          except Exception:
            pass
          if was_int:
            raise ValueError(
              'Device id %s should have been specified as an integer' %
              condition[1])
        self.device_id_conditions.append((c0, device))
      else:
        raise ValueError('Unknown expectation condition: "%s"' % c0)
    else:
      cl = condition.lower()
      if cl in GPU_CONDITIONS:
        self.gpu_conditions.append(cl)
      elif cl in ANGLE_CONDITIONS:
        self.angle_conditions.append(cl)
      elif cl in CMD_DECODER_CONDITIONS:
        self.cmd_decoder_conditions.append(cl)
      else:
        # Delegate to superclass.
        super(GpuExpectation, self).ParseCondition(condition)


class GpuTestExpectations(test_expectations.TestExpectations):
  def __init__(self, url_prefixes=None, is_asan=False):
    super(GpuTestExpectations, self).__init__(
      url_prefixes=url_prefixes, is_asan=is_asan)
    self._cached_browser = None
    self._cached_system_info = None

  def CreateExpectation(self, expectation, pattern, conditions=None,
                        bug=None):
    return GpuExpectation(expectation, pattern, conditions, bug)

  def Flaky(self, pattern, conditions=None, bug=None, max_num_retries=2):
    self._AddExpectation(GpuExpectation(
      'flaky', pattern, conditions=conditions, bug=bug,
      max_num_retries=max_num_retries))

  def GetFlakyRetriesForTest(self, browser, test_url, test_name):
    e = self._GetExpectationObjectForTest(browser, test_url, test_name)
    if e:
      return e.max_num_retries
    return 0

  def _ExpectationAppliesToTest(
      self, expectation, browser, test_url, test_name):
    if not super(GpuTestExpectations, self)._ExpectationAppliesToTest(
        expectation, browser, test_url, test_name):
      return False

    # We'll only get here if the OS and browser type matched the expectation.
    gpu_matches = True
    angle_renderer = ''

    # Fetching the browser's system info is somewhat expensive. Avoid
    # doing it a lot.
    if browser != self._cached_browser:
      self._cached_browser = browser
      self._cached_system_info = None

    if self._cached_system_info is None:
      self._cached_system_info = browser.GetSystemInfo()

    if self._cached_system_info is not None:
      gpu_info = self._cached_system_info.gpu
      gpu_vendor = self._GetGpuVendorString(gpu_info)
      gpu_device_id = self._GetGpuDeviceId(gpu_info)
      gpu_matches = ((not expectation.gpu_conditions and
          not expectation.device_id_conditions) or
          gpu_vendor in expectation.gpu_conditions or
          (gpu_vendor, gpu_device_id) in expectation.device_id_conditions)
      angle_renderer = self.GetANGLERenderer(gpu_info)
      angle_matches = (
        (not expectation.angle_conditions) or
        angle_renderer in expectation.angle_conditions)
      cmd_decoder = self.GetCommandDecoder(gpu_info)
      cmd_decoder_matches = (
        (not expectation.cmd_decoder_conditions) or
        cmd_decoder in expectation.cmd_decoder_conditions)

    return gpu_matches and angle_matches and cmd_decoder_matches

  def _ParseANGLEGpuVendorString(self, device_string):
    if not device_string:
      return None
    # ANGLE's device (renderer) string is of the form:
    # "ANGLE (vendor_string, renderer_string, gl_version profile)"
    match = re.search(r'ANGLE \((.*), .*, .*\)', device_string)
    if match:
      return match.group(1)
    else:
      return None

  def _GetGpuVendorString(self, gpu_info):
    if gpu_info:
      primary_gpu = gpu_info.devices[0]
      if primary_gpu:
        vendor_string = primary_gpu.vendor_string
        angle_vendor_string = self._ParseANGLEGpuVendorString(
          primary_gpu.device_string)
        vendor_id = primary_gpu.vendor_id
        if vendor_id == 0x10DE:
          return 'nvidia'
        elif vendor_id == 0x1002:
          return 'amd'
        elif vendor_id == 0x8086:
          return 'intel'
        elif angle_vendor_string:
          return angle_vendor_string.lower()
        elif vendor_string:
          return vendor_string.split(' ')[0].lower()
    return 'unknown_gpu'

  def _GetANGLEGpuDeviceId(self, device_string):
    if not device_string:
      return None
    # ANGLE's device (renderer) string is of the form:
    # "ANGLE (vendor_string, renderer_string, gl_version profile)"
    match = re.search(r'ANGLE \(.*, (.*), .*\)', device_string)
    if match:
      return match.group(1)
    else:
      return None

  def _GetGpuDeviceId(self, gpu_info):
    if gpu_info:
      primary_gpu = gpu_info.devices[0]
      if primary_gpu:
        return primary_gpu.device_id or \
            self._GetANGLEGpuDeviceId(primary_gpu.device_string) or \
            primary_gpu.device_string
    return 0

  @staticmethod
  def GetANGLERenderer(gpu_info):
    if gpu_info and gpu_info.aux_attributes:
      gl_renderer = gpu_info.aux_attributes.get('gl_renderer')
      if gl_renderer and 'ANGLE' in gl_renderer:
        if 'Direct3D11' in gl_renderer:
          return 'd3d11'
        elif 'Direct3D9' in gl_renderer:
          return 'd3d9'
        elif 'OpenGL ES' in gl_renderer:
          return 'opengles'
        elif 'OpenGL' in gl_renderer:
          return 'opengl'
        elif 'Vulkan' in gl_renderer:
          return 'vulkan'
    return 'no_angle'

  @staticmethod
  def GetCommandDecoder(gpu_info):
    if gpu_info and gpu_info.aux_attributes and \
        gpu_info.aux_attributes.get('passthrough_cmd_decoder', False):
      return 'passthrough'
    return 'no_passthrough'

  def GenerateNewExpectationsFormat(self):
    expectations = ''
    expectations_lines = []

    def _GenerateTestExpectation(exp, conditions, cond=''):
      if not conditions:
        line = ''
        if exp.bug:
          line += 'crbug.com/%d ' % exp.bug
        if cond:
          line += '[ ' + cond + '] '
        line += exp.pattern + ' '
        if exp.expectation == 'flaky':
          line += '[ RetryOnFailure ]'
        elif exp.expectation == 'fail':
          line += '[ Failure ]'
        else:
          line += '[ Skip ]'
        expectations_lines.append(line)
      else:
        if conditions[0]:
          for c in conditions[0]:
            _GenerateTestExpectation(exp, conditions[1:], cond + c + ' ')
        else:
          _GenerateTestExpectation(exp, [], cond)

    conditions_by_type = {
        member: set() for member, attr in inspect.getmembers(
            self.expectations[0]) if type(attr) == list}
    for exp in self.expectations:
      _conds = defaultdict(set)
      for member, attr in inspect.getmembers(exp):
        if member in conditions_by_type:
          tags = []
          for tag in attr:
            if isinstance(tag, tuple):
              try:
                int(tag[1])
                tags.append('%s-0x%x' % tag)
              except Exception:
                tags.append('%s-%s' % tag)
            else:
              tags.append(tag)
          tags = [t.lower().replace(' ', '-').replace('_', '-')  for t in tags]
          key = 'gpu_conditions' if member == 'device_id_conditions' else member
          _conds[key].update(tags)
          conditions_by_type[key].update(tags)
      exp_conditions = sorted(_conds.values(), key=lambda l: -len(l))
      _GenerateTestExpectation(exp, exp_conditions)
    for _, tags in conditions_by_type.items():
      if not tags:
        continue
      expectations += '# tags: [ %s ]\n' % ' '.join(tags)
    expectations += '\n' + '\n'.join(expectations_lines) + '\n'
    return expectations
