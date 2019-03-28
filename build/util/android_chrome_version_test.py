# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from android_chrome_version import GenerateVersionCodes


class _VersionTest(unittest.TestCase):
  """Unittests for the android_chrome_version module.
  """

  EXAMPLE_VERSION_VALUES = {
      'MAJOR': '74',
      'MINOR': '0',
      'BUILD': '3720',
      'PATCH': '0',
  }

  def testGenerateVersionCodesAndroidChrome(self):
    """Assert it gives correct values for standard/example inputs"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    chrome_version_code = output['CHROME_VERSION_CODE']

    self.assertEqual(chrome_version_code, '372000000')

  def testGenerateVersionCodesAndroidChromeModern(self):
    """Assert it gives correct values for standard/example inputs"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    chrome_modern_version_code = output['CHROME_MODERN_VERSION_CODE']

    self.assertEqual(chrome_modern_version_code, '372000001')

  def testGenerateVersionCodesAndroidMonochrome(self):
    """Assert it gives correct values for standard/example inputs"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    monochrome_version_code = output['MONOCHROME_VERSION_CODE']

    self.assertEqual(monochrome_version_code, '372000002')

  def testGenerateVersionCodesAndroidTrichrome(self):
    """Assert it gives correct values for standard/example inputs"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    trichrome_version_code = output['TRICHROME_VERSION_CODE']

    self.assertEqual(trichrome_version_code, '372000003')

  def testGenerateVersionCodesAndroidWebview(self):
    """Assert it gives correct values for standard/example inputs"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)

    webview_version_code = output['WEBVIEW_VERSION_CODE']

    self.assertEqual(webview_version_code, '372000000')

  def testGenerateVersionCodesAndroidNextBuild(self):
    """Assert it handles "next" builds correctly"""
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=True)

    # Get just a sample of values
    chrome_version_code = output['CHROME_VERSION_CODE']
    monochrome_version_code = output['MONOCHROME_VERSION_CODE']
    webview_version_code = output['WEBVIEW_VERSION_CODE']

    self.assertEqual(chrome_version_code, '372100005')
    self.assertEqual(monochrome_version_code, '372100007')
    self.assertEqual(webview_version_code, '372100005')

  def testGenerateVersionCodesAndroidArchArm(self):
    """Assert it handles different architectures correctly.

    Version codes for different builds need to be distinct and maintain a
    certain ordering.
    See docstring on android_chrome_version.ARCH_VERSION_CODE_DIFF for
    reasoning.
    """
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)
    arch_chrome_version_code = output['CHROME_VERSION_CODE']

    self.assertEqual(arch_chrome_version_code, '372000000')

  def testGenerateVersionCodesAndroidArchX86(self):
    """Assert it handles different architectures correctly.

    Version codes for different builds need to be distinct and maintain a
    certain ordering.
    See docstring on android_chrome_version.ARCH_VERSION_CODE_DIFF for
    reasoning.
    """
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='x86', is_next_build=False)
    arch_chrome_version_code = output['CHROME_VERSION_CODE']

    self.assertEqual(arch_chrome_version_code, '372000010')

  def testGenerateVersionCodesAndroidArchMips(self):
    """Assert it handles different architectures correctly.

    Version codes for different builds need to be distinct and maintain a
    certain ordering.
    See docstring on android_chrome_version.ARCH_VERSION_CODE_DIFF for
    reasoning.
    """
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='mipsel', is_next_build=False)
    arch_chrome_version_code = output['CHROME_VERSION_CODE']

    self.assertEqual(arch_chrome_version_code, '372000020')

  def testGenerateVersionCodesAndroidArchArm64(self):
    """Assert it handles different architectures correctly.

    Version codes for different builds need to be distinct and maintain a
    certain ordering.
    See docstring on android_chrome_version.ARCH_VERSION_CODE_DIFF for
    reasoning.
    """
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm64', is_next_build=False)
    arch_chrome_version_code = output['CHROME_VERSION_CODE']

    self.assertEqual(arch_chrome_version_code, '372000030')

  def testGenerateVersionCodesAndroidArchX64(self):
    """Assert it handles different architectures correctly.

    Version codes for different builds need to be distinct and maintain a
    certain ordering.
    See docstring on android_chrome_version.ARCH_VERSION_CODE_DIFF for
    reasoning.
    """
    output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='x64', is_next_build=False)
    arch_chrome_version_code = output['CHROME_VERSION_CODE']

    self.assertEqual(arch_chrome_version_code, '372000060')

  def testGenerateVersionCodesAndroidArchOrderArm(self):
    """Assert it handles different architectures correctly.

    Version codes for different builds need to be distinct and maintain a
    certain ordering.
    See docstring on android_chrome_version.ARCH_VERSION_CODE_DIFF for
    reasoning.

    Test arm-related values.
    """
    arm_output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm', is_next_build=False)
    arm64_output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='arm64', is_next_build=False)

    arm_chrome_version_code = arm_output['CHROME_VERSION_CODE']
    arm64_chrome_version_code = arm64_output['CHROME_VERSION_CODE']

    self.assertLess(arm_chrome_version_code, arm64_chrome_version_code)

  def testGenerateVersionCodesAndroidArchOrderX86(self):
    """Assert it handles different architectures correctly.

    Version codes for different builds need to be distinct and maintain a
    certain ordering.
    See docstring on android_chrome_version.ARCH_VERSION_CODE_DIFF for
    reasoning.

    Test x86-related values.
    """
    x86_output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='x86', is_next_build=False)
    x64_output = GenerateVersionCodes(
        self.EXAMPLE_VERSION_VALUES, arch='x64', is_next_build=False)

    x86_chrome_version_code = x86_output['CHROME_VERSION_CODE']
    x64_chrome_version_code = x64_output['CHROME_VERSION_CODE']

    self.assertLess(x86_chrome_version_code, x64_chrome_version_code)


if __name__ == '__main__':
  unittest.main()
