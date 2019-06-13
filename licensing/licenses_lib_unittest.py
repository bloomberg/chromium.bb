# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the license_lib module."""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.licensing import licenses_lib


class LicenseLibTest(cros_test_lib.TempDirTestCase):
  """Limited tests for license_lib."""

  LICENSE_PUBLIC = 'Gentoo Package Stock'
  LICENSE_CUSTOM = 'Custom'

  def setUp(self):
    """Sets up the filesystem for the tests."""

    # Licenses for testing:
    #   key: Name of the license.
    #     contents: The contents of the license file itself.
    #     dir: Location of the file containing the license text on the FS.
    #     board: The board argument to use when testing the license.
    #     type: The license type (gentoo stock or custom).
    #     [skip_test]: Skip the tests for that license.

    self.licenses = {
        'TPL': {
            'contents': 'Third Party License',
            'dir': 'src/third_party/portage-stable/licenses/TPL',
            'board': 'foo',
            'type': self.LICENSE_PUBLIC,
        },
        'PUB': {
            'contents': 'Public License',
            'dir': 'src/third_party/chromiumos-overlay/licenses/PUB',
            'board': 'foo',
            'type': self.LICENSE_CUSTOM,
        },
        'CCL': {
            'contents': 'ChromeOS Custom License',
            'dir': 'src/private-overlays/chromeos-overlay/licenses/CCL',
            'board': 'chromeos',
            'type': self.LICENSE_CUSTOM,
        },
        'CPL': {
            'contents': 'ChromeOS Partner License',
            'dir': 'src/private-overlays/chromeos-partner-overlay/licenses/CPL',
            'board': 'chromeos-partner',
            'type': self.LICENSE_CUSTOM,
        },
        'FTL': {
            'contents': 'Fake Test License',
            'dir': 'src/overlays/overlay-foo/licenses/FTL',
            'board': 'foo',
            'type': self.LICENSE_CUSTOM,
        },
        'OOR': {
            'contents': 'Out Of Reach',
            'dir': 'src/overlays/overlay-bar/licenses/OOR',
            'skip_test': True,
        },
    }

    # Overlays for testing.
    #   dir: Overlay base directory.
    #   repo-name: The repo-name setting for the overlay.
    #   masters: The masters list for the overlay.

    overlays = [
        {
            'dir': 'src/overlays/overlay-foo',
            'repo-name': 'foo',
            'masters': ['chromiumos', 'portage-stable'],
        },
        {
            'dir': 'src/overlays/overlay-bar',
            'repo-name': 'bar',
            'masters': [],
        },
        {
            'dir': 'src/private-overlays/overlay-foo-private',
            'repo-name': 'foo-private',
            'masters': ['foo', 'chromeos-partner'],
        },
        {
            'dir': 'src/private-overlays/chromeos-overlay',
            'repo-name': 'chromeos',
            'masters': ['chromiumos', 'portage-stable'],
        },
        {
            'dir': 'src/private-overlays/chromeos-partner-overlay',
            'repo-name': 'chromeos-partner',
            'masters': ['chromeos'],
        },
        {
            'dir': 'src/third_party/chromiumos-overlay',
            'repo-name': 'chromiumos',
            'masters': ['portage-stable'],
        },
        {
            'dir': 'src/third_party/portage-stable',
            'repo-name': 'portage-stable',
            'masters': [],
        },
    ]

    # Template for overlay/metadata/layout.conf.
    layout_template = """
repo-name = %(repo_name)s
masters = %(masters)s
"""

    # Convenince variables for ebuild dictionary building.
    foo_eb_dir = os.path.join(self.tempdir, 'src/overlays/overlay-foo/category')
    foo_ec_dir = os.path.join(self.tempdir, 'src/overlays/overlay-foo/eclass')
    _priv_rel_path = 'src/private-overlays/overlay-foo-private/category'
    foopriv_eb_dir = os.path.join(self.tempdir, _priv_rel_path)

    # Ebuilds for testing.
    #   key: Package name of the ebuild.
    #     dir: .ebuild file location.
    #     board: Overlay that contains the ebuild.
    #     license: The license the ebuild uses.

    self.ebuilds = {
        'ftl-pkg': {
            'dir': os.path.join(foo_eb_dir, 'ftl-pkg/ftl-pkg-1.ebuild'),
            'board': 'foo',
            'license': 'FTL',
        },
        'pub-pkg': {
            'dir': os.path.join(foo_eb_dir, 'pub-pkg/pub-pkg-1.ebuild'),
            'board': 'foo',
            'license': 'PUB',
        },
        'pub-cls': {
            'dir': os.path.join(foo_ec_dir, 'pub-cls.eclass'),
            'board': 'foo',
            'license': 'PUB',
        },
        'tpl-pkg': {
            'dir': os.path.join(foo_eb_dir, 'tpl-pkg/tpl-pkg-1.ebuild'),
            'board': 'foo',
            'license': 'TPL',
        },
        'ccl-pkg': {
            'dir': os.path.join(foopriv_eb_dir, 'ccl-pkg/ccl-pkg-1.ebuild'),
            'board': 'foo-private',
            'license': 'CCL',
        },
    }

    # .ebuild content template.
    ebuild_template = 'LICENSE="%(license)s"'

    D = cros_test_lib.Directory
    # Much of the file layout has to be an exact match due to hard coding of
    # a few repos. Prepare for lisp mode.
    file_layout = (
        D('src', (
            D('overlays', (
                D('overlay-foo', (
                    D('category', (
                        D('ftl-pkg', ('ftl-pkg-1.ebuild',)),
                        D('pub-pkg', ('pub-pkg-1.ebuild',)),
                        D('tpl-pkg', ('tpl-pkg-1.ebuild',)),
                    )),
                    D('eclass', ('pub-cls.eclass',)),
                    D('licenses', ('FTL',)),
                    D('metadata', ('layout.conf',)),
                )),
                D('overlay-bar', (
                    D('licenses', ('OOR',)),
                    D('metadata', ('layout.conf',)),
                )),
            )),
            D('private-overlays', (
                D('overlay-foo-private', (
                    D('category', (
                        D('ccl-pkg', ('ccl-pkg-1.ebuild',)),
                    )),
                    D('metadata', ('layout.conf',)),
                )),
                D('chromeos-overlay', (
                    D('licenses', ('CCL',)),
                    D('metadata', ('layout.conf',)),
                )),
                D('chromeos-partner-overlay', (
                    D('licenses', ('CPL',)),
                    D('metadata', ('layout.conf',)),
                )),
            )),
            D('third_party', (
                D('chromiumos-overlay', (
                    D('licenses', ('PUB',)),
                    D('metadata', ('layout.conf',)),
                )),
                D('portage-stable', (
                    D('licenses', ('TPL',)),
                    D('metadata', ('layout.conf',)),
                )),
            )),
        )),
    )
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, file_layout)

    # Write out the files declared above.
    for lic in self.licenses.values():
      osutils.WriteFile(os.path.join(self.tempdir, lic['dir']), lic['contents'])

    for overlay in overlays:
      content = layout_template % {'masters': ' '.join(overlay['masters']),
                                   'repo_name': overlay['repo-name']}
      file_loc = os.path.join(self.tempdir, overlay['dir'], 'metadata',
                              'layout.conf')
      osutils.WriteFile(file_loc, content)

    for name, ebuild in self.ebuilds.items():
      content = ebuild_template % {'license': ebuild['license']}
      self.ebuilds[name]['content'] = content
      osutils.WriteFile(os.path.join(self.tempdir, ebuild['dir']), content)

  def testGetLicenseTypesFromEbuild(self):
    """Tests the fetched license from ebuilds are correct."""
    ebuild_content = self.ebuilds['ftl-pkg']['content']
    overlay_path = os.sep.join(
        self.ebuilds['ftl-pkg']['dir'].split(os.sep)[:-3])

    result = licenses_lib.GetLicenseTypesFromEbuild(ebuild_content,
                                                    overlay_path, self.tempdir)
    expected = ['FTL']
    self.assertEqual(expected, sorted(result))

  def testFindLicenseType(self):
    """Tests the type (gentoo/custom) for licenses are correctly identified."""
    # Doesn't exist anywhere.
    self.assertRaises(AssertionError, licenses_lib.Licensing.FindLicenseType,
                      'DoesNotExist', buildroot=self.tempdir)
    # Not in overlay hierarchy.
    self.assertRaises(AssertionError, licenses_lib.Licensing.FindLicenseType,
                      'OOR', board='foo', buildroot=self.tempdir)
    # Not included in the default search paths.
    self.assertRaises(AssertionError, licenses_lib.Licensing.FindLicenseType,
                      'FTL', buildroot=self.tempdir)

    # Checking each license type is the expected.
    for name, lic in self.licenses.items():
      if lic.get('skip_test', False):
        continue

      result = licenses_lib.Licensing.FindLicenseType(name,
                                                      board=lic['board'],
                                                      buildroot=self.tempdir)
      self.assertEqual(lic['type'], result)

  def testReadSharedLicense(self):
    """Tests the license text is correctly fetched."""
    # Doesn't exist.
    self.assertRaises(AssertionError, licenses_lib.Licensing.ReadSharedLicense,
                      'DoesNotExist', buildroot=self.tempdir)
    # Not in overlay hierarchy.
    self.assertRaises(AssertionError, licenses_lib.Licensing.ReadSharedLicense,
                      'OOR', board='foo', buildroot=self.tempdir)
    # Not included in the default search paths.
    self.assertRaises(AssertionError, licenses_lib.Licensing.ReadSharedLicense,
                      'FTL', buildroot=self.tempdir)

    for name, lic in self.licenses.items():
      if lic.get('skip_test', False):
        continue

      result = licenses_lib.Licensing.ReadSharedLicense(name,
                                                        board=lic['board'],
                                                        buildroot=self.tempdir)
      self.assertEqual(lic['contents'], result)

  def testReadUnknownEncodedFile(self):
    """Validate the fix for crbug.com/654894."""
    bad_license = os.path.join(self.tempdir, 'license.rtf')
    osutils.WriteFile(bad_license, u'Foo\x00Bar')
    result = licenses_lib.ReadUnknownEncodedFile(bad_license)
    self.assertEqual(result, 'FooBar')
