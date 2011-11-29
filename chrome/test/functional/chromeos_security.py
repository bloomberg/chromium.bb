#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional
import pyauto


class ChromeosSecurity(pyauto.PyUITest):
  """Security tests for chrome on ChromeOS.

  Requires ChromeOS to be logged in.
  """
  def setUp(self):
    pyauto.PyUITest.setUp(self)
    baseline_file = os.path.abspath(os.path.join(
        pyauto.PyUITest.DataDir(), 'pyauto_private', 'chromeos',
        'security', 'extension_permission_baseline.txt'))
    self.assertTrue(os.path.exists(baseline_file),
                    msg='Baseline info file does not exist.')
    baseline_info = self.EvalDataFrom(baseline_file)
    self._bundled_crx_directory = baseline_info['BUNDLED_CRX_DIRECTORY']
    self._bundled_crx_baseline = baseline_info['BUNDLED_CRX_BASELINE']
    self._component_extension_baseline = (
        baseline_info['COMPONENT_EXTENSION_BASELINE'])
    if self.GetBrowserInfo()['properties']['is_official']:
      self._component_extension_baseline.extend(
          baseline_info['OFFICIAL_COMPONENT_EXTENSIONS'])

  def ExtraChromeFlags(self):
    """Override default list of extra flags typically used with automation.

    See the default flags used with automation in pyauto.py.
    Chrome flags for this test should be as close to reality as possible.
    """
    return [
      '--homepage=about:blank',
    ]

  def testCannotViewLocalFiles(self):
    """Verify that local files cannot be accessed from the browser."""
    urls_and_titles = {
       'file:///': 'Index of /',
       'file:///etc/': 'Index of /etc/',
       self.GetFileURLForDataPath('title2.html'): 'Title Of Awesomeness',
    }
    for url, title in urls_and_titles.iteritems():
      self.NavigateToURL(url)
      self.assertNotEqual(title, self.GetActiveTabTitle(),
          msg='Could access local file %s.' % url)

  def _AssertPermissionSetIsExpected(self, expected_set, actual_set, perm_type,
                                     full_expected_info, full_actual_info):
    """Asserts that the set of permissions for an extension is expected.

    Args:
      expected_set: A set of permissions that are expected to be present.
      actual_set: A set of permissions that are actually present.
      perm_type: A string describing the type of permission involved.
      full_expected_info: A dictionary fully describing the expected information
                          associated with the given extension.
      full_actual_info: A dictionary fully describing the actual information
                        associated with the given extension.
    """
    def _GetSetDifferenceMessage(expected_set, actual_set):
      strings = []
      for missing_item in expected_set.difference(actual_set):
        strings.append('Missing item: "%s"' % missing_item)
      for extra_item in actual_set.difference(expected_set):
        strings.append('Unexpected (extra) item: "%s"' % extra_item)
      return '\n'.join(strings)

    self.assertEqual(
        expected_set, actual_set,
        msg=('%s do not match for "%s".\n'
             '%s\n'
             'Expected extension info:\n%s'
             '\nActual extension info:\n%s' %
             (perm_type, full_expected_info['name'],
              _GetSetDifferenceMessage(expected_set, actual_set),
              self.pformat(full_expected_info),
              self.pformat(full_actual_info))))

  def _AssertExtensionNamesAreExpected(self, expected_set, actual_set,
                                       ext_type, full_expected_info,
                                       full_actual_info):
    """Asserts that a set of extensions is expected.

    Args:
      expected_set: A set of extension names that are expected to be present.
      actual_set: A set of extension names that are actually present.
      ext_type: A string describing the type of extensions involved.
      full_expected_info: A list of dictionaries describing the expected
                          information for all extensions.
      full_actual_info: A list of dictionaries describing the actual information
                        for all extensions.
    """
    def _GetSetDifferenceMessage(expected_set, actual_set):
      strings = []
      for missing_item in expected_set.difference(actual_set):
        strings.append('Missing item: "%s"' % missing_item)
        located_ext_info = [info for info in full_expected_info if
                            info['name'] == missing_item][0]
        strings.append(self.pformat(located_ext_info))
      for extra_item in actual_set.difference(expected_set):
        strings.append('Unexpected (extra) item: "%s"' % extra_item)
        located_ext_info = [info for info in full_actual_info if
                            info['name'] == extra_item][0]
        strings.append(self.pformat(located_ext_info))
      return '\n'.join(strings)

    self.assertEqual(
        expected_set, actual_set,
        msg='%s names do not match the baseline.\n'
            '%s\n' %
            (ext_type, _GetSetDifferenceMessage(expected_set, actual_set)))

  def _VerifyExtensionPermissions(self, baseline):
    """Ensures extension permissions in the baseline match actual info.

    This function will fail the current test if either (1) an extension named
    in the baseline is not currently installed in Chrome; or (2) the api
    permissions or effective host permissions of an extension in the baseline
    do not match the actual permissions associated with the extension in Chrome.

    Args:
      baseline: A dictionary of expected extension information, containing
                extension names and api/effective host permission info.
    """
    full_ext_actual_info = self.GetExtensionsInfo()
    for ext_expected_info in baseline:
      located_ext_info = [info for info in full_ext_actual_info if
                          info['name'] == ext_expected_info['name']]
      self.assertTrue(
          located_ext_info,
          msg=('Cannot locate extension info for "%s".\n'
               'Expected extension info:\n%s' %
               (ext_expected_info['name'], self.pformat(ext_expected_info))))
      ext_actual_info = located_ext_info[0]
      self._AssertPermissionSetIsExpected(
          set(ext_expected_info['effective_host_permissions']),
          set(ext_actual_info['effective_host_permissions']),
          'Effective host permissions', ext_expected_info, ext_actual_info)
      self._AssertPermissionSetIsExpected(
          set(ext_expected_info['api_permissions']),
          set(ext_actual_info['api_permissions']),
          'API permissions', ext_expected_info, ext_actual_info)

  def testComponentExtensionPermissions(self):
    """Ensures component extension permissions are as expected."""
    expected_names = [ext['name'] for ext in self._component_extension_baseline]
    ext_actual_info = self.GetExtensionsInfo()
    actual_names = [ext['name'] for ext in ext_actual_info if
                    ext['is_component']]
    self._AssertExtensionNamesAreExpected(
        set(expected_names), set(actual_names), 'Component extension',
        self._component_extension_baseline, ext_actual_info)
    self._VerifyExtensionPermissions(self._component_extension_baseline)

  def testBundledCrxPermissions(self):
    """Ensures bundled CRX permissions are as expected."""
    # Verify that each bundled CRX on the device is expected, then install it.
    for file_name in os.listdir(self._bundled_crx_directory):
      if file_name.endswith('.crx'):
        self.assertTrue(
            file_name in [x['crx_file'] for x in self._bundled_crx_baseline],
            msg='Unexpected CRX file: ' + file_name)
        crx_file = os.path.join(self._bundled_crx_directory, file_name)
        self.InstallExtension(crx_file)

    # Verify that the permissions information in the baseline matches the
    # permissions associated with the installed bundled CRX extensions.
    self._VerifyExtensionPermissions(self._bundled_crx_baseline)

  def testNoUnexpectedExtensions(self):
    """Ensures there are no unexpected bundled or component extensions."""
    # Install all bundled extensions on the device.
    for file_name in os.listdir(self._bundled_crx_directory):
      if file_name.endswith('.crx'):
        crx_file = os.path.join(self._bundled_crx_directory, file_name)
        self.InstallExtension(crx_file)

    # Ensure that the set of installed extension names precisely matches the
    # baseline.
    expected_names = [ext['name'] for ext in self._component_extension_baseline]
    expected_names.extend([ext['name'] for ext in self._bundled_crx_baseline])
    ext_actual_info = self.GetExtensionsInfo()
    installed_names = [ext['name'] for ext in ext_actual_info]
    self._AssertExtensionNamesAreExpected(
        set(expected_names), set(installed_names), 'Installed extension',
        self._component_extension_baseline + self._bundled_crx_baseline,
        ext_actual_info)


if __name__ == '__main__':
  pyauto_functional.Main()
