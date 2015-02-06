# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros deploy command."""

# TODO(garnold) Add unit tests for the cros deploy command itself
# (chromium:441551).

from __future__ import print_function

import json
import os

from chromite.cros.commands import cros_deploy
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
try:
  import portage
except ImportError:
  if cros_build_lib.IsInsideChroot():
    raise


class ChromiumOSDeviceHandlerFake(object):
  """Fake for chromite.lib.remote_access.ChomiumOSDeviceHandler."""

  class RemoteAccessFake(object):
    """Fake for chromite.lib.remote_access.RemoteAccess."""

    def __init__(self):
      self.remote_sh_output = None

    def RemoteSh(self, *_args, **_kwargs):
      return cros_build_lib.CommandResult(output=self.remote_sh_output)

  def __init__(self):
    self.agent = self.RemoteAccessFake()


class DbApiFake(object):
  """Fake for Portage dbapi."""

  def __init__(self, pkgs):
    self.pkg_db = {}
    for cpv, slot, rdeps_raw, build_time in pkgs:
      self.pkg_db[cpv] = {
          'SLOT': slot, 'RDEPEND': rdeps_raw, 'BUILD_TIME': build_time}

  def cpv_all(self):
    return self.pkg_db.keys()

  def aux_get(self, cpv, keys):
    pkg_info = self.pkg_db[cpv]
    return [pkg_info[key] for key in keys]


class PortageTreeFake(object):
  """Fake for Portage tree."""

  def __init__(self, dbapi):
    self.dbapi = dbapi


class TestInstallPackageScanner(cros_test_lib.MockTestCase):
  """Test the update package scanner."""
  _BOARD = 'foo_board'
  _BUILD_ROOT = '/build/%s' % _BOARD
  _VARTREE = [
      ('foo/app1-1.2.3-r4', '0', 'foo/app2 !foo/app3', '1413309336'),
      ('foo/app2-4.5.6-r7', '0', '', '1413309336'),
      ('foo/app4-2.0.0-r1', '0', 'foo/app1 foo/app5', '1413309336'),
      ('foo/app5-3.0.7-r3', '0', '', '1413309336'),
  ]

  def setUp(self):
    """Patch imported modules."""
    self.PatchObject(cros_build_lib, 'GetChoice', return_value=0)
    self.device = ChromiumOSDeviceHandlerFake()
    self.scanner = cros_deploy.InstallPackageScanner(self._BOARD,
                                                     self._BUILD_ROOT)

  def SetupVartree(self, vartree_pkgs):
    self.device.agent.remote_sh_output = json.dumps(vartree_pkgs)

  def SetupBintree(self, bintree_pkgs):
    bintree = PortageTreeFake(DbApiFake(bintree_pkgs))
    build_root = os.path.join(self._BUILD_ROOT, '')
    portage_db = {build_root: {'bintree': bintree}}
    self.PatchObject(portage, 'create_trees', return_value=portage_db)

  def ValidatePkgs(self, actual, expected, constraints=None):
    # Containing exactly the same packages.
    self.assertEquals(sorted(expected), sorted(actual))
    # Packages appear in the right order.
    if constraints is not None:
      for needs, needed in constraints:
        self.assertGreater(actual.index(needs), actual.index(needed))

  def testRunUpdatedVersion(self):
    self.SetupVartree(self._VARTREE)
    app1 = 'foo/app1-1.2.5-r4'
    self.SetupBintree([
        (app1, '0', 'foo/app2 !foo/app3', '1413309336'),
        ('foo/app2-4.5.6-r7', '0', '', '1413309336'),
    ])
    installs, listed, num_updates = self.scanner.Run(
        self.device, '/', ['app1'], True, True, True)
    self.ValidatePkgs(installs, [app1])
    self.ValidatePkgs(listed, [app1])
    self.assertEquals(num_updates, 1)

  def testRunUpdatedBuildTime(self):
    self.SetupVartree(self._VARTREE)
    app1 = 'foo/app1-1.2.3-r4'
    self.SetupBintree([
        (app1, '0', 'foo/app2 !foo/app3', '1413309350'),
        ('foo/app2-4.5.6-r7', '0', '', '1413309336'),
    ])
    installs, listed, num_updates = self.scanner.Run(
        self.device, '/', ['app1'], True, True, True)
    self.ValidatePkgs(installs, [app1])
    self.ValidatePkgs(listed, [app1])
    self.assertEquals(num_updates, 1)

  def testRunExistingDepUpdated(self):
    self.SetupVartree(self._VARTREE)
    app1 = 'foo/app1-1.2.5-r2'
    app2 = 'foo/app2-4.5.8-r3'
    self.SetupBintree([
        (app1, '0', 'foo/app2 !foo/app3', '1413309350'),
        (app2, '0', '', '1413309350'),
    ])
    installs, listed, num_updates = self.scanner.Run(
        self.device, '/', ['app1'], True, True, True)
    self.ValidatePkgs(installs, [app1, app2], constraints=[(app1, app2)])
    self.ValidatePkgs(listed, [app1])
    self.assertEquals(num_updates, 2)

  def testRunMissingDepUpdated(self):
    self.SetupVartree(self._VARTREE)
    app1 = 'foo/app1-1.2.5-r2'
    app6 = 'foo/app6-1.0.0-r1'
    self.SetupBintree([
        (app1, '0', 'foo/app2 !foo/app3 foo/app6', '1413309350'),
        ('foo/app2-4.5.6-r7', '0', '', '1413309336'),
        (app6, '0', '', '1413309350'),
    ])
    installs, listed, num_updates = self.scanner.Run(
        self.device, '/', ['app1'], True, True, True)
    self.ValidatePkgs(installs, [app1, app6], constraints=[(app1, app6)])
    self.ValidatePkgs(listed, [app1])
    self.assertEquals(num_updates, 1)

  def testRunExistingRevDepUpdated(self):
    self.SetupVartree(self._VARTREE)
    app1 = 'foo/app1-1.2.5-r2'
    app4 = 'foo/app4-2.0.1-r3'
    self.SetupBintree([
        (app1, '0', 'foo/app2 !foo/app3', '1413309350'),
        (app4, '0', 'foo/app1 foo/app5', '1413309350'),
        ('foo/app5-3.0.7-r3', '0', '', '1413309336'),
    ])
    installs, listed, num_updates = self.scanner.Run(
        self.device, '/', ['app1'], True, True, True)
    self.ValidatePkgs(installs, [app1, app4], constraints=[(app4, app1)])
    self.ValidatePkgs(listed, [app1])
    self.assertEquals(num_updates, 2)

  def testRunMissingRevDepNotUpdated(self):
    self.SetupVartree(self._VARTREE)
    app1 = 'foo/app1-1.2.5-r2'
    app6 = 'foo/app6-1.0.0-r1'
    self.SetupBintree([
        (app1, '0', 'foo/app2 !foo/app3', '1413309350'),
        (app6, '0', 'foo/app1', '1413309350'),
    ])
    installs, listed, num_updates = self.scanner.Run(
        self.device, '/', ['app1'], True, True, True)
    self.ValidatePkgs(installs, [app1])
    self.ValidatePkgs(listed, [app1])
    self.assertEquals(num_updates, 1)

  def testRunTransitiveDepsUpdated(self):
    self.SetupVartree(self._VARTREE)
    app1 = 'foo/app1-1.2.5-r2'
    app2 = 'foo/app2-4.5.8-r3'
    app4 = 'foo/app4-2.0.0-r1'
    app5 = 'foo/app5-3.0.8-r2'
    self.SetupBintree([
        (app1, '0', 'foo/app2 !foo/app3', '1413309350'),
        (app2, '0', '', '1413309350'),
        (app4, '0', 'foo/app1 foo/app5', '1413309350'),
        (app5, '0', '', '1413309350'),
    ])
    installs, listed, num_updates = self.scanner.Run(
        self.device, '/', ['app1'], True, True, True)
    self.ValidatePkgs(installs, [app1, app2, app4, app5],
                      constraints=[(app1, app2), (app4, app1), (app4, app5)])
    self.ValidatePkgs(listed, [app1])
    self.assertEquals(num_updates, 4)

  def testRunDisjunctiveDepsExistingUpdated(self):
    self.SetupVartree(self._VARTREE)
    app1 = 'foo/app1-1.2.5-r2'
    self.SetupBintree([
        (app1, '0', '|| ( foo/app6 foo/app2 ) !foo/app3', '1413309350'),
        ('foo/app2-4.5.6-r7', '0', '', '1413309336'),
    ])
    installs, listed, num_updates = self.scanner.Run(
        self.device, '/', ['app1'], True, True, True)
    self.ValidatePkgs(installs, [app1])
    self.ValidatePkgs(listed, [app1])
    self.assertEquals(num_updates, 1)

  def testRunDisjunctiveDepsDefaultUpdated(self):
    self.SetupVartree(self._VARTREE)
    app1 = 'foo/app1-1.2.5-r2'
    app7 = 'foo/app7-1.0.0-r1'
    self.SetupBintree([
        (app1, '0', '|| ( foo/app6 foo/app7 ) !foo/app3', '1413309350'),
        (app7, '0', '', '1413309350'),
    ])
    installs, listed, num_updates = self.scanner.Run(
        self.device, '/', ['app1'], True, True, True)
    self.ValidatePkgs(installs, [app1, app7], constraints=[(app1, app7)])
    self.ValidatePkgs(listed, [app1])
    self.assertEquals(num_updates, 1)
