# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for gclient.py."""

from __future__ import print_function

import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import gclient
from chromite.lib import osutils


class TestGclientWriteConfigFile(
    cros_build_lib_unittest.RunCommandTempDirTestCase):
  """Unit tests for gclient.WriteConfigFile."""

  _TEST_CWD = '/work/chrome'

  def _AssertGclientConfigSpec(self, expected_spec):
    if cros_build_lib.HostIsCIBuilder():
      expected_spec += "cache_dir = '/b/git-cache'\n"
    self.rc.assertCommandContains(('gclient', 'config', '--spec',
                                   expected_spec),
                                  cwd=self._TEST_CWD)

  def _CreateGclientTemplate(self, template_content):
    template_path = os.path.join(self.tempdir, 'gclient_template')
    osutils.WriteFile(template_path, template_content)
    return template_path

  def testChromiumSpec(self):
    """Test WriteConfigFile with chromium checkout and no revision."""
    gclient.WriteConfigFile('gclient', self._TEST_CWD, False, None)
    self._AssertGclientConfigSpec("""solutions = [{'custom_deps': {},
  'custom_vars': {},
  'deps_file': '.DEPS.git',
  'name': 'src',
  'url': 'https://chromium.googlesource.com/chromium/src.git'}]
""")

  def testChromeSpec(self):
    """Test WriteConfigFile with chrome checkout and no revision."""
    gclient.WriteConfigFile('gclient', self._TEST_CWD, True, None)
    self._AssertGclientConfigSpec("""solutions = [{'custom_deps': {},
  'custom_vars': {},
  'deps_file': '.DEPS.git',
  'name': 'src',
  'url': 'https://chromium.googlesource.com/chromium/src.git'},
 {'custom_deps': {},
  'custom_vars': {},
  'deps_file': '.DEPS.git',
  'name': 'src-internal',
  'url': 'https://chrome-internal.googlesource.com/chrome/src-internal.git'}]
""")

  def testChromiumSpecWithGitHash(self):
    """Test WriteConfigFile with chromium checkout at a given git revision."""
    gclient.WriteConfigFile('gclient', self._TEST_CWD, False,
                            '7becbe4afb42b3301d42149d7d1cade017f150ff')
    self._AssertGclientConfigSpec("""solutions = [{'custom_deps': {},
  'custom_vars': {},
  'deps_file': '.DEPS.git',
  'name': 'src',
  'url': 'https://chromium.googlesource.com/chromium/src.git@7becbe4afb42b3301d42149d7d1cade017f150ff'}]
""")

  def testChromeSpecWithGitHash(self):
    """Test WriteConfigFile with chrome checkout at a given git revision."""
    gclient.WriteConfigFile('gclient', self._TEST_CWD, True,
                            '7becbe4afb42b3301d42149d7d1cade017f150ff')
    self._AssertGclientConfigSpec("""solutions = [{'custom_deps': {},
  'custom_vars': {},
  'deps_file': '.DEPS.git',
  'name': 'src',
  'url': 'https://chromium.googlesource.com/chromium/src.git@7becbe4afb42b3301d42149d7d1cade017f150ff'},
 {'custom_deps': {},
  'custom_vars': {},
  'deps_file': '.DEPS.git',
  'name': 'src-internal',
  'url': 'https://chrome-internal.googlesource.com/chrome/src-internal.git'}]
""")

  def testChromiumSpecWithReleaseTag(self):
    """Test WriteConfigFile with chromium checkout at a given release tag."""
    gclient.WriteConfigFile('gclient', self._TEST_CWD, False, '41.0.2270.0')
    self._AssertGclientConfigSpec("""solutions = [{'custom_deps': {},
  'custom_vars': {},
  'deps_file': '.DEPS.git',
  'name': 'src',
  'url': 'https://chromium.googlesource.com/chromium/src.git@refs/tags/41.0.2270.0'}]
""")

  def testChromeSpecWithReleaseTag(self):
    """Test WriteConfigFile with chrome checkout at a given release tag."""
    gclient.WriteConfigFile('gclient', self._TEST_CWD, True, '41.0.2270.0')
    self._AssertGclientConfigSpec("""solutions = [{'custom_deps': {},
  'custom_vars': {},
  'deps_file': 'releases/41.0.2270.0/.DEPS.git',
  'name': 'CHROME_DEPS',
  'url': 'https://chrome-internal.googlesource.com/chrome/tools/buildspec.git'}]
""")

  def testChromeSpecWithGclientTemplate(self):
    """Test WriteConfigFile with chrome checkout with a gclient template."""
    template_path = self._CreateGclientTemplate("""solutions = [
  {
    'name': 'src',
    'custom_deps': {'dep1': '1'},
    'custom_vars': {'var1': 'test1', 'var2': 'test2'},
  },
  { 'name': 'no-vars', 'custom_deps': {'dep2': '2', 'dep3': '3'} },
  { 'name': 'no-deps', 'custom_vars': {'var3': 'a', 'var4': 'b'} }
]""")
    gclient.WriteConfigFile('gclient', self._TEST_CWD, True,
                            '7becbe4afb42b3301d42149d7d1cade017f150ff',
                            template=template_path)
    self._AssertGclientConfigSpec("""solutions = [{'custom_deps': {'dep1': '1'},
  'custom_vars': {'var1': 'test1', 'var2': 'test2'},
  'deps_file': '.DEPS.git',
  'name': 'src',
  'url': 'https://chromium.googlesource.com/chromium/src.git@7becbe4afb42b3301d42149d7d1cade017f150ff'},
 {'custom_deps': {'dep2': '2', 'dep3': '3'}, 'name': 'no-vars'},
 {'custom_vars': {'var3': 'a', 'var4': 'b'}, 'name': 'no-deps'},
 {'custom_deps': {},
  'custom_vars': {},
  'deps_file': '.DEPS.git',
  'name': 'src-internal',
  'url': 'https://chrome-internal.googlesource.com/chrome/src-internal.git'}]
""")
