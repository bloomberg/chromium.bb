# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch

OS_MODIFIERS = ['win', 'xp', 'vista', 'win7',
                'mac', 'leopard', 'snowleopard', 'lion', 'mountainlion',
                'mavericks', 'yosemite', 'linux', 'chromeos', 'android']
GPU_MODIFIERS = ['amd', 'arm', 'broadcom', 'hisilicon', 'intel', 'imagination',
                 'nvidia', 'qualcomm', 'vivante']
CONFIG_MODIFIERS = ['debug', 'release']

class Expectation(object):
  """Represents a single test expectation for a page.

  Subclass this class and call super.__init__ last in your constructor
  in order to add new user-defined conditions. The conditions are
  parsed at the end of this class's constructor, so be careful not to
  overwrite the results of the constructor call!
  """

  def __init__(self, expectation, pattern, conditions=None, bug=None):
    self.expectation = expectation.lower()
    self.name_pattern = pattern
    self.url_pattern = pattern
    self.bug = bug

    self.os_conditions = []
    self.gpu_conditions = []
    self.config_conditions = []
    self.device_id_conditions = []

    # Make sure that non-absolute paths are searchable
    if not '://' in self.url_pattern:
      self.url_pattern = '*/' + self.url_pattern

    if conditions:
      for c in conditions:
        self.ParseCondition(c)

  def ParseCondition(self, condition):
    """Parses a single test expectation condition.

    Can be overridden to handle new types of conditions. Call the
    superclass's implementation of ParseCondition at the end of your
    subclass if you don't handle the condition. The base
    implementation will raise an exception if the condition is
    unsupported.

    Valid expectation conditions are:

    Operating systems:
      win, xp, vista, win7, mac, leopard, snowleopard, lion,
      mountainlion, mavericks, yosemite, linux, chromeos, android

    GPU vendors:
      amd, arm, broadcom, hisilicon, intel, imagination, nvidia,
      qualcomm, vivante

    Specific GPUs can be listed as a tuple with vendor name and device ID.
    Examples: ('nvidia', 0x1234), ('arm', 'Mali-T604')
    Device IDs must be paired with a GPU vendor.

    """
    if isinstance(condition, tuple):
      c0 = condition[0].lower()
      if c0 in GPU_MODIFIERS:
        self.device_id_conditions.append((c0, condition[1]))
      else:
        raise ValueError('Unknown expectation condition: "%s"' % c0)
    else:
      cl = condition.lower()
      if cl in OS_MODIFIERS:
        self.os_conditions.append(cl)
      elif cl in GPU_MODIFIERS:
        self.gpu_conditions.append(cl)
      elif cl in CONFIG_MODIFIERS:
        self.config_conditions.append(cl)
      else:
        raise ValueError('Unknown expectation condition: "%s"' % cl)


class TestExpectations(object):
  """A class which defines the expectations for a page set test execution"""

  def __init__(self):
    self.expectations = []
    self.SetExpectations()

  def SetExpectations(self):
    """Called on creation. Override to set up custom expectations."""
    pass

  def Fail(self, url_pattern, conditions=None, bug=None):
    self._Expect('fail', url_pattern, conditions, bug)

  def Skip(self, url_pattern, conditions=None, bug=None):
    self._Expect('skip', url_pattern, conditions, bug)

  def _Expect(self, expectation, url_pattern, conditions=None, bug=None):
    self.expectations.append(self.CreateExpectation(expectation, url_pattern,
                                                    conditions, bug))

  def CreateExpectation(self, expectation, url_pattern, conditions=None,
                        bug=None):
    return Expectation(expectation, url_pattern, conditions, bug)

  def _GetExpectationObjectForPage(self, browser, page):
    for e in self.expectations:
      if self.ExpectationAppliesToPage(e, browser, page):
        return e
    return None

  def GetExpectationForPage(self, browser, page):
    e = self._GetExpectationObjectForPage(browser, page)
    if e:
      return e.expectation
    return 'pass'

  def ExpectationAppliesToPage(self, expectation, browser, page):
    """Defines whether the given expectation applies to the given page.

    Override this in subclasses to add more conditions. Call the
    superclass's implementation first, and return false if it returns
    false.

    Args:
      expectation: an instance of a subclass of Expectation, created
          by a call to CreateExpectation.
      browser: the currently running browser.
      page: the page to be run.
    """
    matches_url = fnmatch.fnmatch(page.url, expectation.url_pattern)
    matches_name = page.name and fnmatch.fnmatch(page.name,
                                                 expectation.name_pattern)
    if not (matches_url or matches_name):
      return False

    platform = browser.platform
    os_matches = (not expectation.os_conditions or
        platform.GetOSName() in expectation.os_conditions or
        platform.GetOSVersionName() in expectation.os_conditions)

    if not os_matches:
      return False

    gpu_matches = True

    # TODO(kbr): factor out all of the GPU-related conditions into
    # GpuTestExpectations, including unit tests.
    if browser.supports_system_info:
      gpu_info = browser.GetSystemInfo().gpu
      gpu_vendor = self._GetGpuVendorString(gpu_info)
      gpu_device_id = self._GetGpuDeviceId(gpu_info)
      gpu_matches = ((not expectation.gpu_conditions and
          not expectation.device_id_conditions) or
          gpu_vendor in expectation.gpu_conditions or
          (gpu_vendor, gpu_device_id) in expectation.device_id_conditions)

    return gpu_matches

  def _GetGpuVendorString(self, gpu_info):
    if gpu_info:
      primary_gpu = gpu_info.devices[0]
      if primary_gpu:
        vendor_string = primary_gpu.vendor_string.lower()
        vendor_id = primary_gpu.vendor_id
        if vendor_string:
          return vendor_string.split(' ')[0]
        elif vendor_id == 0x10DE:
          return 'nvidia'
        elif vendor_id == 0x1002:
          return 'amd'
        elif vendor_id == 0x8086:
          return 'intel'

    return 'unknown_gpu'

  def _GetGpuDeviceId(self, gpu_info):
    if gpu_info:
      primary_gpu = gpu_info.devices[0]
      if primary_gpu:
        return primary_gpu.device_id or primary_gpu.device_string

    return 0
