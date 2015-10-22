# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests import test_expectations

ANGLE_CONDITIONS = ['d3d9', 'd3d11', 'opengl']

GPU_CONDITIONS = ['amd', 'arm', 'broadcom', 'hisilicon', 'intel', 'imagination',
                  'nvidia', 'qualcomm', 'vivante']

class GpuExpectation(test_expectations.Expectation):
  def __init__(self, expectation, pattern, conditions=None, bug=None,
               max_num_retries=0):
    self.gpu_conditions = []
    self.device_id_conditions = []
    self.angle_conditions = []
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
      d3d9, d3d11, opengl

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
        self.device_id_conditions.append((c0, condition[1]))
      else:
        raise ValueError('Unknown expectation condition: "%s"' % c0)
    else:
      cl = condition.lower()
      if cl in GPU_CONDITIONS:
        self.gpu_conditions.append(cl)
      elif cl in ANGLE_CONDITIONS:
        self.angle_conditions.append(cl)
      else:
        # Delegate to superclass.
        super(GpuExpectation, self).ParseCondition(condition)


class GpuTestExpectations(test_expectations.TestExpectations):
  def CreateExpectation(self, expectation, pattern, conditions=None,
                        bug=None):
    return GpuExpectation(expectation, pattern, conditions, bug)

  def Flaky(self, pattern, conditions=None, bug=None, max_num_retries=2):
    self._AddExpectation(GpuExpectation(
      'flaky', pattern, conditions=conditions, bug=bug,
      max_num_retries=max_num_retries))

  def GetFlakyRetriesForPage(self, browser, page):
    e = self._GetExpectationObjectForPage(browser, page)
    if e:
      return e.max_num_retries
    return 0

  def ExpectationAppliesToPage(self, expectation, browser, page):
    if not super(GpuTestExpectations, self).ExpectationAppliesToPage(
        expectation, browser, page):
      return False

    # We'll only get here if the OS and browser type matched the expectation.
    gpu_matches = True
    angle_renderer = ''

    if browser.supports_system_info:
      gpu_info = browser.GetSystemInfo().gpu
      gpu_vendor = self._GetGpuVendorString(gpu_info)
      gpu_device_id = self._GetGpuDeviceId(gpu_info)
      gpu_matches = ((not expectation.gpu_conditions and
          not expectation.device_id_conditions) or
          gpu_vendor in expectation.gpu_conditions or
          (gpu_vendor, gpu_device_id) in expectation.device_id_conditions)
      angle_renderer = self._GetANGLERenderer(gpu_info)
      angle_matches = (
        (not expectation.angle_conditions) or
        angle_renderer in expectation.angle_conditions)

    return gpu_matches and angle_matches

  def _GetGpuVendorString(self, gpu_info):
    if gpu_info:
      primary_gpu = gpu_info.devices[0]
      if primary_gpu:
        vendor_string = primary_gpu.vendor_string.lower()
        vendor_id = primary_gpu.vendor_id
        if vendor_id == 0x10DE:
          return 'nvidia'
        elif vendor_id == 0x1002:
          return 'amd'
        elif vendor_id == 0x8086:
          return 'intel'
        elif vendor_string:
          return vendor_string.split(' ')[0]
    return 'unknown_gpu'

  def _GetGpuDeviceId(self, gpu_info):
    if gpu_info:
      primary_gpu = gpu_info.devices[0]
      if primary_gpu:
        return primary_gpu.device_id or primary_gpu.device_string
    return 0

  def _GetANGLERenderer(self, gpu_info):
    if gpu_info and gpu_info.aux_attributes:
      gl_renderer = gpu_info.aux_attributes.get('gl_renderer')
      if gl_renderer:
        if 'Direct3D11' in gl_renderer:
          return 'd3d11'
        elif 'Direct3D9' in gl_renderer:
          return 'd3d9'
        elif 'OpenGL' in gl_renderer:
          return 'opengl'
    return ''
