# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""brillo test: Run unit tests or integration tests."""

from __future__ import print_function

from chromite.cli import command
from chromite.lib import blueprint_lib
from chromite.lib import chroot_util
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import portage_util
from chromite.lib import workon_helper


_UNITTEST_PREFIX = 'unittest:'


@command.CommandDecorator('test')
class TestCommand(command.CliCommand):
  """Run tests for a given blueprint."""

  def __init__(self, options):
    super(TestCommand, self).__init__(options)
    blueprint = blueprint_lib.Blueprint(self.options.blueprint)
    self.sysroot = cros_build_lib.GetSysroot(blueprint.FriendlyName())

  @classmethod
  def AddParser(cls, parser):
    super(cls, TestCommand).AddParser(parser)
    parser.add_argument('blueprint', type='blueprint_path',
                        help='Blueprint to test.')
    parser.add_argument('packages', nargs='+',
                        help='List of packages to test.')
    parser.add_argument('--verbose', action='store_true',
                        help='Print the test output, even if the tests '
                        'succeed.')

  def _GetMatchingPackages(self, package_list):
    """Match |package_list| to packages available on the system.

    Args:
      package_list: Package strings passed by the user.

    Returns:
      A tuple containing the set of packages that match an ebuild and the set of
      packages that do not map to any ebuild.
    """
    packages = set(package_list)
    helper = workon_helper.WorkonHelper(self.sysroot)
    available_packages = set(helper.ListAtoms(use_all=True))
    available_packages |= set(a.split('/')[1] for a in available_packages)

    matching = packages & available_packages
    return matching, packages - matching

  def Run(self):
    """Run the tests."""
    self.options.Freeze()
    commandline.RunInsideChroot(self)

    packages, non_matching = self._GetMatchingPackages(self.options.packages)
    if non_matching:
      cros_build_lib.Die('No packages matching: %s', ' '.join(non_matching))

    packages_with_tests = portage_util.PackagesWithTest(self.sysroot, packages)
    packages_without_tests = packages - packages_with_tests
    if packages_without_tests:
      logging.warning('Ignored the following packages because they were '
                      'missing tests:')
      for p in packages_without_tests:
        logging.warning(p)

    if not packages_with_tests:
      logging.error('Nothing to test.')
      return

    try:
      chroot_util.RunUnittests(self.sysroot, packages_with_tests,
                               verbose=self.options.verbose, retries=0)
    except cros_build_lib.RunCommandError as e:
      cros_build_lib.Die('Unit tests failed: %s' % e)
