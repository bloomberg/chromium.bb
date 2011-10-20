#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_portage_upgrade.py."""

import cStringIO
import exceptions
import optparse
import os
import re
import sys
import unittest

import mox

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
import chromite.lib.cros_build_lib as cros_lib
import cros_portage_upgrade as cpu
import parallel_emerge
import portage.package.ebuild.config as portcfg
import portage.tests.resolver.ResolverPlayground as respgnd

# Regex to find the character sequence to turn text red (used for errors).
ERROR_PREFIX = re.compile('^\033\[1;31m')

DEFAULT_PORTDIR = '/usr/portage'

# Configuration for generating a temporary valid ebuild hierarchy.
# TODO(mtennant): Wrap this mechanism to create multiple overlays.
# ResolverPlayground sets up a default profile with ARCH=x86, so
# other architectures are irrelevant for now.
DEFAULT_ARCH = 'x86'
EBUILDS = {
  "dev-libs/A-1": {"RDEPEND" : "dev-libs/B"},
  "dev-libs/A-2": {"RDEPEND" : "dev-libs/B"},
  "dev-libs/B-1": {"RDEPEND" : "dev-libs/C"},
  "dev-libs/B-2": {"RDEPEND" : "dev-libs/C"},
  "dev-libs/C-1": {},
  "dev-libs/C-2": {},
  "dev-libs/D-1": {"RDEPEND": "!dev-libs/E"},
  "dev-libs/D-2": {},
  "dev-libs/D-3": {},
  "dev-libs/E-2": {"RDEPEND": "!dev-libs/D"},
  "dev-libs/E-3": {},

  "dev-libs/F-1": {"SLOT": "1"},
  "dev-libs/F-2": {"SLOT": "2"},
  "dev-libs/F-2-r1": {"SLOT": "2",
                      "KEYWORDS": "~amd64 ~x86 ~arm",
                      },

  "dev-apps/X-1": {
    "EAPI": "3",
    "SLOT": "0",
    "KEYWORDS": "amd64 arm x86",
    "RDEPEND": "=dev-libs/C-1",
    },
  "dev-apps/Y-2": {
    "EAPI": "3",
    "SLOT": "0",
    "KEYWORDS": "amd64 arm x86",
    "RDEPEND": "=dev-libs/C-2",
    },

  "chromeos-base/flimflam-0.0.1-r228": {
    "EAPI" : "2",
    "SLOT" : "0",
    "KEYWORDS" : "amd64 x86 arm",
    "RDEPEND" : ">=dev-libs/D-2",
    },
  "chromeos-base/flimflam-0.0.2-r123": {
    "EAPI" : "2",
    "SLOT" : "0",
    "KEYWORDS" : "~amd64 ~x86 ~arm",
    "RDEPEND" : ">=dev-libs/D-3",
    },
  "chromeos-base/libchrome-57098-r4": {
    "EAPI" : "2",
    "SLOT" : "0",
    "KEYWORDS" : "amd64 x86 arm",
    "RDEPEND" : ">=dev-libs/E-2",
    },
  "chromeos-base/libcros-1": {
    "EAPI" : "2",
    "SLOT" : "0",
    "KEYWORDS" : "amd64 x86 arm",
    "RDEPEND" : "dev-libs/B dev-libs/C chromeos-base/flimflam",
    "DEPEND" :
    "dev-libs/B dev-libs/C chromeos-base/flimflam chromeos-base/libchrome",
    },

  "virtual/libusb-0"         : {
    "EAPI" :"2", "SLOT" : "0",
    "RDEPEND" :
    "|| ( >=dev-libs/libusb-0.1.12-r1:0 dev-libs/libusb-compat " +
    ">=sys-freebsd/freebsd-lib-8.0[usb] )"},
  "virtual/libusb-1"         : {
    "EAPI" :"2", "SLOT" : "1",
    "RDEPEND" : ">=dev-libs/libusb-1.0.4:1"},
  "dev-libs/libusb-0.1.13"   : {},
  "dev-libs/libusb-1.0.5"    : {"SLOT":"1"},
  "dev-libs/libusb-compat-1" : {},
  "sys-freebsd/freebsd-lib-8": {"IUSE" : "+usb"},

  "sys-fs/udev-164"          : {"EAPI" : "1", "RDEPEND" : "virtual/libusb:0"},

  "virtual/jre-1.5.0"        : {
    "SLOT" : "1.5",
    "RDEPEND" : "|| ( =dev-java/sun-jre-bin-1.5.0* =virtual/jdk-1.5.0* )"},
  "virtual/jre-1.5.0-r1"     : {
    "SLOT" : "1.5",
    "RDEPEND" : "|| ( =dev-java/sun-jre-bin-1.5.0* =virtual/jdk-1.5.0* )"},
  "virtual/jre-1.6.0"        : {
    "SLOT" : "1.6",
    "RDEPEND" : "|| ( =dev-java/sun-jre-bin-1.6.0* =virtual/jdk-1.6.0* )"},
  "virtual/jre-1.6.0-r1"     : {
    "SLOT" : "1.6",
    "RDEPEND" : "|| ( =dev-java/sun-jre-bin-1.6.0* =virtual/jdk-1.6.0* )"},
  "virtual/jdk-1.5.0"        : {
    "SLOT" : "1.5",
    "RDEPEND" : "|| ( =dev-java/sun-jdk-1.5.0* dev-java/gcj-jdk )"},
  "virtual/jdk-1.5.0-r1"     : {
    "SLOT" : "1.5",
    "RDEPEND" : "|| ( =dev-java/sun-jdk-1.5.0* dev-java/gcj-jdk )"},
  "virtual/jdk-1.6.0"        : {
    "SLOT" : "1.6",
    "RDEPEND" : "|| ( =dev-java/icedtea-6* =dev-java/sun-jdk-1.6.0* )"},
  "virtual/jdk-1.6.0-r1"     : {
    "SLOT" : "1.6",
    "RDEPEND" : "|| ( =dev-java/icedtea-6* =dev-java/sun-jdk-1.6.0* )"},
  "dev-java/gcj-jdk-4.5"     : {},
  "dev-java/gcj-jdk-4.5-r1"  : {},
  "dev-java/icedtea-6.1"     : {},
  "dev-java/icedtea-6.1-r1"  : {},
  "dev-java/sun-jdk-1.5"     : {"SLOT" : "1.5"},
  "dev-java/sun-jdk-1.6"     : {"SLOT" : "1.6"},
  "dev-java/sun-jre-bin-1.5" : {"SLOT" : "1.5"},
  "dev-java/sun-jre-bin-1.6" : {"SLOT" : "1.6"},

  "dev-java/ant-core-1.8"   : {"DEPEND"  : ">=virtual/jdk-1.4"},
  "dev-db/hsqldb-1.8"       : {"RDEPEND" : ">=virtual/jre-1.6"},
  }

WORLD = [
  "dev-libs/A",
  "dev-libs/D",
  "virtual/jre",
  ]

INSTALLED = {
  "dev-libs/A-1": {},
  "dev-libs/B-1": {},
  "dev-libs/C-1": {},
  "dev-libs/D-1": {},

  "virtual/jre-1.5.0"       : {
    "SLOT" : "1.5",
    "RDEPEND" : "|| ( =virtual/jdk-1.5.0* =dev-java/sun-jre-bin-1.5.0* )"},
  "virtual/jre-1.6.0"       : {
    "SLOT" : "1.6",
    "RDEPEND" : "|| ( =virtual/jdk-1.6.0* =dev-java/sun-jre-bin-1.6.0* )"},
  "virtual/jdk-1.5.0"       : {
    "SLOT" : "1.5",
    "RDEPEND" : "|| ( =dev-java/sun-jdk-1.5.0* dev-java/gcj-jdk )"},
  "virtual/jdk-1.6.0"       : {
    "SLOT" : "1.6",
    "RDEPEND" : "|| ( =dev-java/icedtea-6* =dev-java/sun-jdk-1.6.0* )"},
  "dev-java/gcj-jdk-4.5"    : {},
  "dev-java/icedtea-6.1"    : {},

  "virtual/libusb-0"         : {
    "EAPI" :"2", "SLOT" : "0",
    "RDEPEND" :
    "|| ( >=dev-libs/libusb-0.1.12-r1:0 dev-libs/libusb-compat " +
    ">=sys-freebsd/freebsd-lib-8.0[usb] )"},
  }

# For verifying dependency graph results
GOLDEN_DEP_GRAPHS = {
  "dev-libs/A-2" : { "needs" : { "dev-libs/B-2" : "runtime" },
                     "action" : "merge" },
  "dev-libs/B-2" : { "needs" : { "dev-libs/C-2" : "runtime" } },
  "dev-libs/C-2" : { "needs" : { } },
  "dev-libs/D-2" : { "needs" : { } },
  "dev-libs/E-3" : { "needs" : { } },
  "chromeos-base/libcros-1" : { "needs" : {
    "dev-libs/B-2" : "runtime/buildtime",
    "dev-libs/C-2" : "runtime/buildtime",
    "chromeos-base/libchrome-57098-r4" : "buildtime",
    "chromeos-base/flimflam-0.0.1-r228" : "runtime/buildtime"
    } },
  "chromeos-base/flimflam-0.0.1-r228" : { "needs" : {
    "dev-libs/D-2" : "runtime"
    } },
  "chromeos-base/libchrome-57098-r4" : { "needs" : {
    "dev-libs/E-3" : "runtime"
    } },
  }

# For verifying dependency list results
GOLDEN_DEP_LISTS = {
  "dev-libs/A" : ['dev-libs/A-2', 'dev-libs/B-2', 'dev-libs/C-2'],
  "dev-libs/B" : ['dev-libs/B-2', 'dev-libs/C-2'],
  "dev-libs/C" : ['dev-libs/C-2'],
  "virtual/libusb" : ['virtual/libusb-1', 'dev-libs/libusb-1.0.5'],
  "chromeos-base/libcros" : ['chromeos-base/libcros-1',
                             'chromeos-base/libchrome-57098-r4',
                             'dev-libs/E-3',
                             'dev-libs/B-2',
                             'dev-libs/C-2',
                             'chromeos-base/flimflam-0.0.1-r228',
                             'dev-libs/D-2',
                             ],
  }


def _GetGoldenDepsList(pkg):
  """Retrieve the golden dependency list for |pkg| from GOLDEN_DEP_LISTS."""
  return GOLDEN_DEP_LISTS.get(pkg, None)


def _VerifyDepsGraph(deps_graph, pkg):
  """Verfication function for Mox to validate deps graph for |pkg|."""
  if deps_graph is None:
    print "Error: no dependency graph passed into _GetPreOrderDepGraph"
    return False

  if type(deps_graph) != dict:
    print "Error: dependency graph is expected to be a dict.  Instead: "
    print repr(deps_graph)
    return False

  validated = True

  # Verify size
  golden_deps_list = _GetGoldenDepsList(pkg)
  if golden_deps_list == None:
    print("Error: golden dependency list not configured for %s package" %
          (pkg))
    validated = False
  elif len(deps_graph) != len(golden_deps_list):
    print("Error: expected %d dependencies for %s package, not %d" %
          (len(golden_deps_list), pkg, len(deps_graph)))
    validated = False

  # Verify dependencies, by comparing them to GOLDEN_DEP_GRAPHS
  for p in deps_graph:
    golden_pkg_info = None
    try:
      golden_pkg_info = GOLDEN_DEP_GRAPHS[p]
    except KeyError:
      print("Error: golden dependency graph not configured for %s package" %
            (p))
      validated = False
      continue

    pkg_info = deps_graph[p]
    for key in golden_pkg_info:
      golden_value = golden_pkg_info[key]
      value = pkg_info[key]
      if not value == golden_value:
        print("Error: while verifying '%s' value for %s package,"
              " expected:\n%r\nBut instead found:\n%r"
              % (key, p, golden_value, value))
        validated = False

  if not validated:
    print("Error: dependency graph for %s is not as expected.  Instead:\n%r" %
          (pkg, deps_graph))

  return validated


def _GenDepsGraphVerifier(pkg):
  """Generate a graph verification function for the given package."""
  return lambda deps_graph: _VerifyDepsGraph(deps_graph, pkg)


def _IsErrorLine(line):
  """Return True if |line| has prefix associated with error output."""
  return ERROR_PREFIX.search(line)

def _SetUpEmerge(world=None):
  """Prepare the temporary ebuild playground and emerge variables.

  This leverages test code in existing Portage modules to create an ebuild
  hierarchy.  This can be a little slow."""

  # TODO(mtennant): Support multiple overlays?  This essentially
  # creates just a default overlay.
  # Also note that ResolverPlayground assumes ARCH=x86 for the
  # default profile it creates.
  if world is None:
    world = WORLD
  playground = respgnd.ResolverPlayground(ebuilds=EBUILDS,
                                          installed=INSTALLED,
                                          world=world)

  # Set all envvars needed by emerge, since --board is being skipped.
  eroot = _GetPlaygroundRoot(playground)
  os.environ["PORTAGE_CONFIGROOT"] = eroot
  os.environ["ROOT"] = eroot
  os.environ["PORTDIR"] = "%s%s" % (eroot, DEFAULT_PORTDIR)

  return playground

def _GetPlaygroundRoot(playground):
  """Get the temp dir playground is using as ROOT."""
  eroot = playground.eroot
  if eroot[-1:] == '/':
    eroot = eroot[:-1]
  return eroot

def _GetPortageDBAPI(playground):
  portroot = playground.settings["ROOT"]
  porttree = playground.trees[portroot]['porttree']
  return porttree.dbapi

def _TearDownEmerge(playground):
  """Delete the temporary ebuild playground files."""
  try:
    playground.cleanup()
  except AttributeError:
    pass

# Use this to configure Upgrader using standard command
# line options and arguments.
def _ParseCmdArgs(cmdargs):
  """Returns (options, args) tuple."""
  parser = cpu._CreateOptParser()
  return parser.parse_args(args=cmdargs)

UpgraderSlotDefaults = {
    '_curr_arch':   DEFAULT_ARCH,
    '_curr_board':  'some_board',
    '_unstable_ok': False,
    '_verbose':     False,
    }
def _MockUpgrader(mox, cmdargs=None, **kwargs):
  """Set up a mocked Upgrader object with the given args."""
  upgrader = mox.CreateMock(cpu.Upgrader)

  # Initialize each attribute with default value.
  for slot in cpu.Upgrader.__slots__:
    value = UpgraderSlotDefaults.get(slot, None)
    upgrader.__setattr__(slot, value)

  # Initialize with command line if given.
  if cmdargs:
    (options, args) = _ParseCmdArgs(cmdargs)
    cpu.Upgrader.__init__(upgrader, options, args)

  # Override Upgrader attributes if requested.
  for slot in cpu.Upgrader.__slots__:
    value = None
    if slot in kwargs:
      upgrader.__setattr__(slot, kwargs[slot])

  return upgrader

######################
### EmergeableTest ###
######################

class EmergeableTest(mox.MoxTestBase):
  """Test Upgrader._AreEmergeable."""

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def _TestAreEmergeable(self, cpvlist, expect,
                         stable_only=True, debug=False,
                         world=None):
    """Test the Upgrader._AreEmergeable method.

    |cpvlist| and |stable_only| are passed to _AreEmergeable.
    |expect| is boolean, expected return value of _AreEmergeable
    |debug| requests that emerge output in _AreEmergeable be shown.
    |world| is list of lines to override default world contents.
    """

    cmdargs = ['--upgrade'] + cpvlist
    if not stable_only:
      cmdargs = ['--unstable-ok'] + cmdargs
    mocked_upgrader = _MockUpgrader(self.mox, cmdargs=cmdargs)
    playground = _SetUpEmerge(world=world)

    # Add test-specific mocks/stubs

    # Replay script
    envvars = cpu.Upgrader._GenPortageEnvvars(mocked_upgrader,
                                              mocked_upgrader._curr_arch,
                                              not stable_only)
    mocked_upgrader._GenPortageEnvvars(mocked_upgrader._curr_arch,
                                       not stable_only).AndReturn(envvars)
    mocked_upgrader._GetBoardCmd('emerge').AndReturn('emerge')
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._AreEmergeable(mocked_upgrader, cpvlist, stable_only)
    self.mox.VerifyAll()

    (code, cmd, output) = result
    if debug or code != expect:
      print("\nTest ended with success==%r (expected==%r)" % (code, expect))
      print("Emerge output:\n%s" % output)

    self.assertEquals(code, expect)

    _TearDownEmerge(playground)

  def testAreEmergeableOnePkg(self):
    """Should pass, one cpv target."""
    cpvlist = ['dev-libs/A-1']
    return self._TestAreEmergeable(cpvlist, True)

  def testAreEmergeableTwoPkgs(self):
    """Should pass, two cpv targets."""
    cpvlist = ['dev-libs/A-1', 'dev-libs/B-1']
    return self._TestAreEmergeable(cpvlist, True)

  def testAreEmergeableOnePkgTwoVersions(self):
    """Should fail, targets two versions of same package."""
    cpvlist = ['dev-libs/A-1', 'dev-libs/A-2']
    return self._TestAreEmergeable(cpvlist, False)

  def testAreEmergeableStableFlimFlam(self):
    """Should pass, target stable version of pkg."""
    cpvlist = ['chromeos-base/flimflam-0.0.1-r228']
    return self._TestAreEmergeable(cpvlist, True)

  def testAreEmergeableUnstableFlimFlam(self):
    """Should fail, target unstable version of pkg."""
    cpvlist = ['chromeos-base/flimflam-0.0.2-r123']
    return self._TestAreEmergeable(cpvlist, False)

  def testAreEmergeableUnstableFlimFlamUnstableOk(self):
    """Should pass, target unstable version of pkg with stable_only=False."""
    cpvlist = ['chromeos-base/flimflam-0.0.2-r123']
    return self._TestAreEmergeable(cpvlist, True, stable_only=False)

  def testAreEmergeableBlockedPackages(self):
    """Should fail, targets have blocking deps on each other."""
    cpvlist = ['dev-libs/D-1', 'dev-libs/E-2']
    return self._TestAreEmergeable(cpvlist, False)

  def testAreEmergeableBlockedByInstalledPkg(self):
    """Should fail because of installed D-1 pkg."""
    cpvlist = ['dev-libs/E-2']
    return self._TestAreEmergeable(cpvlist, False)

  def testAreEmergeableNotBlockedByInstalledPkgNotInWorld(self):
    """Should pass because installed D-1 pkg not in world."""
    cpvlist = ['dev-libs/E-2']
    return self._TestAreEmergeable(cpvlist, True, world=[])

  def testAreEmergeableSamePkgDiffSlots(self):
    """Should pass, same package but different slots."""
    cpvlist = ['dev-libs/F-1', 'dev-libs/F-2']
    return self._TestAreEmergeable(cpvlist, True)

  def testAreEmergeableTwoPackagesIncompatibleDeps(self):
    """Should fail, targets depend on two versions of same pkg."""
    cpvlist = ['dev-apps/X-1', 'dev-apps/Y-2']
    return self._TestAreEmergeable(cpvlist, False)

###################
### UtilityTest ###
###################

class UtilityTest(mox.MoxTestBase):
  """Test Upgrader methods: _SplitEBuildPath, _GenPortageEnvvars"""

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  #
  # _GetBoardCmd
  #

  def _TestGetBoardCmd(self, cmd, board):
    """Test Upgrader._GetBoardCmd."""
    mocked_upgrader = _MockUpgrader(self.mox, _curr_board=board)

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._GetBoardCmd(mocked_upgrader, cmd)
    self.mox.VerifyAll()

    return result

  def testGetBoardCmdKnownCmds(self):
    board = 'x86-alex'
    for cmd in ['emerge', 'equery', 'portageq']:
      result = self._TestGetBoardCmd(cmd, cpu.Upgrader.HOST_BOARD)
      self.assertEquals(result, cmd)
      result = self._TestGetBoardCmd(cmd, board)
      self.assertEquals(result, '%s-%s' % (cmd, board))

  def testGetBoardCmdUnknownCmd(self):
    board = 'x86-alex'
    cmd = 'foo'
    result = self._TestGetBoardCmd(cmd, cpu.Upgrader.HOST_BOARD)
    self.assertEquals(result, cmd)
    result = self._TestGetBoardCmd(cmd, board)
    self.assertEquals(result, cmd)

  #
  # _GenPortageEnvvars testing
  #

  def _TestGenPortageEnvvars(self, arch, unstable_ok,
                             portdir=None, portage_configroot=None):
    """Testing the behavior of the Upgrader._GenPortageEnvvars method."""
    mocked_upgrader = _MockUpgrader(self.mox)

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._GenPortageEnvvars(mocked_upgrader,
                                             arch, unstable_ok,
                                             portdir, portage_configroot)
    self.mox.VerifyAll()

    keyw = arch
    if unstable_ok:
      keyw = arch + ' ~' + arch

    self.assertEquals(result['ACCEPT_KEYWORDS'], keyw)
    if portdir is None:
      self.assertFalse('PORTDIR' in result)
    else:
      self.assertEquals(result['PORTDIR'], portdir)
    if portage_configroot is None:
      self.assertFalse('PORTAGE_CONFIGROOT' in result)
    else:
      self.assertEquals(result['PORTAGE_CONFIGROOT'], portage_configroot)

  def testGenPortageEnvvars1(self):
    self._TestGenPortageEnvvars('arm', False)

  def testGenPortageEnvvars2(self):
    self._TestGenPortageEnvvars('x86', True)

  def testGenPortageEnvvars3(self):
    self._TestGenPortageEnvvars('x86', True,
                                portdir='/foo/bar',
                                portage_configroot='/bar/foo')

  #
  # _SplitEBuildPath testing
  #

  def _TestSplitEBuildPath(self, ebuild_path, golden_result):
    """Test the behavior of the Upgrader._SplitEBuildPath method."""
    mocked_upgrader = _MockUpgrader(self.mox)

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._SplitEBuildPath(mocked_upgrader,
                                           ebuild_path)
    self.assertEquals(result, golden_result)
    self.mox.VerifyAll()

  def testSplitEBuildPath1(self):
    self._TestSplitEBuildPath('/foo/bar/portage/dev-libs/A/A-2.ebuild',
                              ('portage', 'dev-libs', 'A', 'A-2'))

  def testSplitEBuildPath2(self):
    self._TestSplitEBuildPath('/foo/ooo/ccc/ppp/ppp-1.2.3-r123.ebuild',
                              ('ooo', 'ccc', 'ppp', 'ppp-1.2.3-r123'))


#######################
### TreeInspectTest ###
#######################

class TreeInspectTest(mox.MoxTestBase):
  """Test Upgrader methods: _FindCurrentCPV, _FindUpstreamCPV"""

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def _GenerateTestInput(self, category, pkg_name, ver_rev,
                         path_prefix=DEFAULT_PORTDIR):
    """Return tuple (ebuild_path, cpv, cp)."""
    ebuild_path = None
    cpv = None
    if ver_rev:
      ebuild_path = '%s/%s/%s/%s-%s.ebuild' % (path_prefix,
                                               category, pkg_name,
                                               pkg_name, ver_rev)
      cpv = '%s/%s-%s' % (category, pkg_name, ver_rev)
    cp = '%s/%s' % (category, pkg_name)
    return (ebuild_path, cpv, cp)

  #
  # _FindUpstreamCPV testing
  #

  def _TestFindUpstreamCPV(self, pkg_arg, ebuild_expect, unstable_ok=False):
    """Test Upgrader._FindUpstreamCPV

    This points _FindUpstreamCPV at the ResolverPlayground as if it is
    the upstream tree.
    """

    playground = _SetUpEmerge()
    eroot = _GetPlaygroundRoot(playground)
    mocked_upgrader = _MockUpgrader(self.mox, _curr_board=None,
                                    _upstream_repo=eroot,
                                    )

    # Add test-specific mocks/stubs

    # Replay script
    envvars = cpu.Upgrader._GenPortageEnvvars(mocked_upgrader,
                                              mocked_upgrader._curr_arch,
                                              unstable_ok,
                                              portdir=eroot,
                                              portage_configroot=eroot)
    portage_configroot = mocked_upgrader._emptydir
    mocked_upgrader._GenPortageEnvvars(mocked_upgrader._curr_arch,
                                       unstable_ok,
                                       portdir=mocked_upgrader._upstream_repo,
                                       portage_configroot=portage_configroot,
                                       ).AndReturn(envvars)

    if ebuild_expect:
      ebuild_path = eroot + ebuild_expect
      split_path = cpu.Upgrader._SplitEBuildPath(mocked_upgrader, ebuild_path)
      mocked_upgrader._SplitEBuildPath(ebuild_path).AndReturn(split_path)
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._FindUpstreamCPV(mocked_upgrader, pkg_arg,
                                           unstable_ok)
    self.mox.VerifyAll()

    self.assertTrue(bool(ebuild_expect) == bool(result))

    _TearDownEmerge(playground)

    return result

  def testFindUpstreamA2(self):
    (ebuild, cpv, cp) = self._GenerateTestInput(category='dev-libs',
                                                pkg_name='A',
                                                ver_rev='2')
    result = self._TestFindUpstreamCPV(cp, ebuild)
    self.assertEquals(result, cpv)

  def testFindUpstreamAAA(self):
    (ebuild, cpv, cp) = self._GenerateTestInput(category='dev-apps',
                                                pkg_name='AAA',
                                                ver_rev=None)
    result = self._TestFindUpstreamCPV(cp, ebuild)
    self.assertEquals(result, cpv)

  def testFindUpstreamF(self):
    (ebuild, cpv, cp) = self._GenerateTestInput(category='dev-libs',
                                                pkg_name='F',
                                                ver_rev='2')
    result = self._TestFindUpstreamCPV(cp, ebuild)
    self.assertEquals(result, cpv)

  def testFindUpstreamFlimflam(self):
    """Should find 0.0.1-r228 because more recent flimflam unstable."""
    (ebuild, cpv, cp) = self._GenerateTestInput(category='chromeos-base',
                                                pkg_name='flimflam',
                                                ver_rev='0.0.1-r228')
    result = self._TestFindUpstreamCPV(cp, ebuild)
    self.assertEquals(result, cpv)

  def testFindUpstreamFlimflamUnstable(self):
    """Should find 0.0.2-r123 because of unstable_ok."""
    (ebuild, cpv, cp) = self._GenerateTestInput(category='chromeos-base',
                                                pkg_name='flimflam',
                                                ver_rev='0.0.2-r123')
    result = self._TestFindUpstreamCPV(cp, ebuild, unstable_ok=True)
    self.assertEquals(result, cpv)

  #
  # _FindCurrentCPV testing
  #

  def _TestFindCurrentCPV(self, pkg_arg, ebuild_expect):
    """Test Upgrader._FindCurrentCPV

    This test points Upgrader._FindCurrentCPV to the ResolverPlayground
    tree as if it is the local source.
    """

    mocked_upgrader = _MockUpgrader(self.mox, _curr_board=None)
    playground = _SetUpEmerge()
    eroot = _GetPlaygroundRoot(playground)

    # Add test-specific mocks/stubs

    # Replay script
    envvars = cpu.Upgrader._GenPortageEnvvars(mocked_upgrader,
                                              mocked_upgrader._curr_arch,
                                              False)
    mocked_upgrader._GenPortageEnvvars(mocked_upgrader._curr_arch,
                                       False).AndReturn(envvars)
    mocked_upgrader._GetBoardCmd('equery').AndReturn('equery')

    if ebuild_expect:
      ebuild_path = eroot + ebuild_expect
      split_path = cpu.Upgrader._SplitEBuildPath(mocked_upgrader, ebuild_path)
      mocked_upgrader._SplitEBuildPath(ebuild_path).AndReturn(split_path)

    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._FindCurrentCPV(mocked_upgrader, pkg_arg)
    self.mox.VerifyAll()

    _TearDownEmerge(playground)

    return result

  def testFindCurrentA(self):
    """Should find dev-libs/A-2."""
    (ebuild, cpv, cp) = self._GenerateTestInput(category='dev-libs',
                                                pkg_name='A',
                                                ver_rev='2')
    result = self._TestFindCurrentCPV(cp, ebuild)
    self.assertEquals(result, cpv)

  def testFindCurrentAAA(self):
    """Should find None, because dev-libs/AAA does not exist in tree."""
    (ebuild, cpv, cp) = self._GenerateTestInput(category='dev-libs',
                                                pkg_name='AAA',
                                                ver_rev=None)
    result = self._TestFindCurrentCPV(cp, ebuild)
    self.assertEquals(result, cpv)

  def testFindCurrentF(self):
    """Should find dev-libs/F-2."""
    (ebuild, cpv, cp) = self._GenerateTestInput(category='dev-libs',
                                                pkg_name='F',
                                                ver_rev='2')
    result = self._TestFindCurrentCPV(cp, ebuild)
    self.assertEquals(result, cpv)

  def testFindCurrentFlimflam(self):
    """Should find 0.0.1-r228 because more recent flimflam unstable."""
    (ebuild, cpv, cp) = self._GenerateTestInput(category='chromeos-base',
                                                pkg_name='flimflam',
                                                ver_rev='0.0.1-r228')
    result = self._TestFindCurrentCPV(cp, ebuild)
    self.assertEquals(result, cpv)

####################
### RunBoardTest ###
####################

class RunBoardTest(mox.MoxTestBase):
  """Test Upgrader.RunBoard.

  This tests at a very high level, just testing the flow of RunBoard.
  """

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def _TestRunBoard(self, infolist, upgrade=False, staged_changes=False):
    """Test Upgrader.RunBoard."""

    targetlist = [info['arg'] for info in infolist]
    local_infolist = [info for info in infolist if 'cpv' in info]
    upstream_only_infolist = [info for info in infolist if 'cpv' not in info]

    cmdargs = targetlist
    if upgrade:
      cmdargs = ['--upgrade'] + cmdargs
    mocked_upgrader = _MockUpgrader(self.mox, cmdargs=cmdargs,
                                    _curr_board=None)
    board = 'runboard_testboard'

    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(cpu.Upgrader, '_FindBoardArch')

    # Replay script
    mocked_upgrader._SaveStatusOnStableRepo().AndReturn(None)
    cpu.Upgrader._FindBoardArch(board).AndReturn('x86')
    upgrade_mode = cpu.Upgrader._IsInUpgradeMode(mocked_upgrader)
    mocked_upgrader._IsInUpgradeMode().AndReturn(upgrade_mode)
    mocked_upgrader._AnyChangesStaged().AndReturn(staged_changes)
    if (staged_changes):
      mocked_upgrader._StashChanges()

    mocked_upgrader._ResolveAndVerifyArgs(targetlist,
                                          upgrade_mode).AndReturn(infolist)
    mocked_upgrader._GetCurrentVersions(infolist).AndReturn(infolist)
    mocked_upgrader._FinalizeLocalInfolist(infolist).AndReturn([])

    if upgrade_mode:
      mocked_upgrader._FinalizeUpstreamInfolist(
        upstream_only_infolist).AndReturn([])

    mocked_upgrader._UnstashAnyChanges()
    mocked_upgrader._UpgradePackages(mox.IgnoreArg())

    mocked_upgrader._DropAnyStashedChanges()

    self.mox.ReplayAll()

    # Verify
    cpu.Upgrader.RunBoard(mocked_upgrader, board)
    self.mox.VerifyAll()

  def testTBD(self):
    target_infolist = [{'arg':   'dev-libs/A',
                        'cpv':   'dev-libs/A-1',
                        'upstream_cpv': 'dev-libs/A-2',
                        },
                       ]
    return self._TestRunBoard(target_infolist)

  def testTBD2(self):
    target_infolist = [{'arg':   'dev-libs/A',
                        'cpv':   'dev-libs/A-1',
                        'upstream_cpv': 'dev-libs/A-2',
                        },
                       ]
    return self._TestRunBoard(target_infolist, upgrade=True)

  def testTBD3(self):
    target_infolist = [{'arg':   'dev-libs/A',
                        'cpv':   'dev-libs/A-1',
                        'upstream_cpv': 'dev-libs/A-2',
                        },
                       ]
    return self._TestRunBoard(target_infolist, upgrade=True,
                              staged_changes=True)

####################
### UpgraderTest ###
####################

# TODO: This test class no longer works.  Replace its pieces one by one.
# Disabled but kept until replaced.

class UpgraderTest(): # (mox.MoxTestBase):
  """Test the Upgrader class from cros_portage_upgrade."""

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def _GetParallelEmergeArgv(self, mocked_upgrader):
    return cpu.Upgrader._GenParallelEmergeArgv(mocked_upgrader)

  #
  # _GetCurrentVersions testing (defunct - to be replaced)
  #

  def _TestGetCurrentVersions(self, pkg):
    """Test the behavior of the Upgrader._GetCurrentVersions method.

    This basically confirms that it uses the parallel_emerge module to
    assemble the expected dependency graph."""
    mocked_upgrader = self._MockUpgrader(board=None, package=pkg, verbose=False)
    pm_argv = self._GetParallelEmergeArgv(mocked_upgrader)
    self._SetUpEmerge()

    # Add test-specific mocks/stubs.
    self.mox.StubOutWithMock(cpu.Upgrader, '_GetPreOrderDepGraph')

    # Replay script
    verifier = _GenDepsGraphVerifier(pkg)
    mocked_upgrader._GenParallelEmergeArgv().AndReturn(pm_argv)
    mocked_upgrader._SetPortTree(mox.IsA(portcfg.config), mox.IsA(dict))
    cpu.Upgrader._GetPreOrderDepGraph(mox.Func(verifier)).AndReturn(['ignore'])
    self.mox.ReplayAll()

    # Verify
    graph = cpu.Upgrader._GetCurrentVersions(mocked_upgrader)
    self.mox.VerifyAll()

    self._TearDownEmerge()

  def testGetCurrentVersionsDevLibsA(self):
    return self._TestGetCurrentVersions('dev-libs/A')

  def testGetCurrentVersionsDevLibsB(self):
    return self._TestGetCurrentVersions('dev-libs/B')

  def testGetCurrentVersionsCrosbaseLibcros(self):
    return self._TestGetCurrentVersions('chromeos-base/libcros')

  #
  # _GetPreOrderDepGraph testing (defunct - to be replaced)
  #

  def _TestGetPreOrderDepGraph(self, pkg):
    """Test the behavior of the Upgrader._GetPreOrderDepGraph method."""

    mocked_upgrader = self._MockUpgrader(board=None, package=pkg, verbose=False)
    pm_argv = self._GetParallelEmergeArgv(mocked_upgrader)
    self._SetUpEmerge()

    # Replay script
    self.mox.ReplayAll()

    # Verify
    deps = parallel_emerge.DepGraphGenerator()
    deps.Initialize(pm_argv)
    deps_tree, deps_info = deps.GenDependencyTree()
    deps_graph = deps.GenDependencyGraph(deps_tree, deps_info)

    deps_list = cpu.Upgrader._GetPreOrderDepGraph(deps_graph)
    golden_deps_list = _GetGoldenDepsList(pkg)
    self.assertEquals(deps_list, golden_deps_list)
    self.mox.VerifyAll()

    self._TearDownEmerge()

  def testGetPreOrderDepGraphDevLibsA(self):
    return self._TestGetPreOrderDepGraph('dev-libs/A')

  def testGetPreOrderDepGraphDevLibsC(self):
    return self._TestGetPreOrderDepGraph('dev-libs/C')

  def testGetPreOrderDepGraphVirtualLibusb(self):
    return self._TestGetPreOrderDepGraph('virtual/libusb')

  def testGetPreOrderDepGraphCrosbaseLibcros(self):
    return self._TestGetPreOrderDepGraph('chromeos-base/libcros')

  #
  # _GetInfoListWithOverlays testing
  #

  def _TestGetInfoListWithOverlays(self, pkg):
    """Test the behavior of the Upgrader._GetInfoListWithOverlays method."""

    self._SetUpEmerge()

    # Add test-specific mocks/stubs

    # Replay script, if any
    self.mox.ReplayAll()

    # Verify
    cpvlist = _GetGoldenDepsList(pkg)
    (options, args) = self._MockUpgraderOptions(board=None,
                                                package=pkg,
                                                verbose=False)
    upgrader = cpu.Upgrader(options, args)
    upgrader._SetPortTree(self._playground.settings, self._playground.trees)

    cpvinfolist = upgrader._GetInfoListWithOverlays(cpvlist)
    self.mox.VerifyAll()

    # Verify the overlay that was found for each cpv.  Always "portage" for now,
    # because that is what is created by the temporary ebuild creator.
    # TODO(mtennant): Support multiple overlays somehow.
    for cpvinfo in cpvinfolist:
      self.assertEquals('portage', cpvinfo['overlay'])

    self._TearDownEmerge()

  def testGetInfoListWithOverlaysDevLibsA(self):
    return self._TestGetInfoListWithOverlays('dev-libs/A')

  def testGetInfoListWithOverlaysCrosbaseLibcros(self):
    return self._TestGetInfoListWithOverlays('chromeos-base/libcros')

################
### MainTest ###
################

class MainTest(mox.MoxTestBase):
  """Test argument handling at the main method level."""

  def setUp(self):
    """Setup for all tests in this class."""
    mox.MoxTestBase.setUp(self)

  def _StartCapturingOutput(self):
    """Begin capturing stdout and stderr."""
    self._stdout = sys.stdout
    self._stderr = sys.stderr
    sys.stdout = self._stdout_cap = cStringIO.StringIO()
    sys.stderr = self._stderr_cap = cStringIO.StringIO()

  def _RetrieveCapturedOutput(self):
    """Return captured output so far as (stdout, stderr) tuple."""
    try:
      return (self._stdout_cap.getvalue(), self._stderr_cap.getvalue())
    except AttributeError:
      # This will happen if output capturing isn't on.
      return None

  def _StopCapturingOutput(self):
    """Stop capturing stdout and stderr."""
    try:
      sys.stdout = self._stdout
      sys.stderr = self._stderr
    except AttributeError:
      # This will happen if output capturing wasn't on.
      pass

  def _PrepareArgv(self, *args):
    """Prepare command line for calling cros_portage_upgrade.main"""
    sys.argv = [ re.sub("_unittest", "", sys.argv[0]) ]
    sys.argv.extend(args)

  def _AssertOutputEndsInError(self, stdout):
    """Return True if |stdout| ends with an error message."""
    lastline = [ln for ln in stdout.split('\n') if ln][-1]
    self.assertTrue(_IsErrorLine(lastline),
                    msg="expected output to end in error line, but "
                    "_IsErrorLine says this line is not an error:\n%s" %
                    lastline)

  def _AssertCPUMain(self, cpu, expect_zero):
    """Run cpu.main() and assert exit value is expected.

    If |expect_zero| is True, assert exit value = 0.  If False,
    assert exit value != 0.
    """
    try:
      cpu.main()
    except exceptions.SystemExit, e:
      if expect_zero:
        self.assertEquals(e.args[0], 0,
                          msg="expected call to main() to exit cleanly, "
                          "but it exited with code %d" % e.args[0])
      else:
        self.assertNotEquals(e.args[0], 0,
                             msg="expected call to main() to exit with "
                             "failure code, but exited with code 0 instead.")

  def testHelp(self):
    """Test that --help is functioning"""
    self._PrepareArgv("--help")

    # Capture stdout/stderr so it can be verified later
    self._StartCapturingOutput()

    # Running with --help should exit with code==0
    try:
      cpu.main()
    except exceptions.SystemExit, e:
      self.assertEquals(e.args[0], 0)

    # Verify that a message beginning with "Usage: " was printed
    (stdout, stderr) = self._RetrieveCapturedOutput()
    self._StopCapturingOutput()
    self.assertTrue(stdout.startswith("Usage: "))

  def testMissingBoard(self):
    """Test that running without --board exits with an error."""
    self._PrepareArgv("")

    # Capture stdout/stderr so it can be verified later
    self._StartCapturingOutput()

    # Running without --board should exit with code!=0
    try:
      cpu.main()
    except exceptions.SystemExit, e:
      self.assertNotEquals(e.args[0], 0)

    (stdout, stderr) = self._RetrieveCapturedOutput()
    self._StopCapturingOutput()
    self._AssertOutputEndsInError(stdout)

  def testBoardWithoutPackage(self):
    """Test that running without a package argument exits with an error."""
    self._PrepareArgv("--board=any-board")

    # Capture stdout/stderr so it can be verified later
    self._StartCapturingOutput()

    # Running without a package should exit with code!=0
    self._AssertCPUMain(cpu, expect_zero=False)

    # Verify that an error message was printed.
    (stdout, stderr) = self._RetrieveCapturedOutput()
    self._StopCapturingOutput()
    self._AssertOutputEndsInError(stdout)

  def testHostWithoutPackage(self):
    """Test that running without a package argument exits with an error."""
    self._PrepareArgv("--host")

    # Capture stdout/stderr so it can be verified later
    self._StartCapturingOutput()

    # Running without a package should exit with code!=0
    self._AssertCPUMain(cpu, expect_zero=False)

    # Verify that an error message was printed.
    (stdout, stderr) = self._RetrieveCapturedOutput()
    self._StopCapturingOutput()
    self._AssertOutputEndsInError(stdout)

  def testFlowStatusReportOneBoard(self):
    """Test main flow for basic one-board status report."""
    self._StartCapturingOutput()

    self.mox.StubOutWithMock(cpu.Upgrader, 'PreRunChecks')
    self.mox.StubOutWithMock(cpu, '_BoardIsSetUp')
    self.mox.StubOutWithMock(cpu.Upgrader, 'PrepareToRun')
    self.mox.StubOutWithMock(cpu.Upgrader, 'RunBoard')
    self.mox.StubOutWithMock(cpu.Upgrader, 'RunCompleted')
    self.mox.StubOutWithMock(cpu.Upgrader, 'WriteTableFiles')

    cpu.Upgrader.PreRunChecks()
    cpu._BoardIsSetUp('any-board').AndReturn(True)
    cpu.Upgrader.PrepareToRun()
    cpu.Upgrader.RunBoard('any-board')
    cpu.Upgrader.RunCompleted()
    cpu.Upgrader.WriteTableFiles(csv='/dev/null')

    self.mox.ReplayAll()

    self._PrepareArgv("--board=any-board", "--to-csv=/dev/null", "any-package")
    self._AssertCPUMain(cpu, expect_zero=True)
    self.mox.VerifyAll()

    self._StopCapturingOutput()

  def testFlowStatusReportOneBoardNotSetUp(self):
    """Test main flow for basic one-board status report."""
    self._StartCapturingOutput()

    self.mox.StubOutWithMock(cpu.Upgrader, 'PreRunChecks')
    self.mox.StubOutWithMock(cpu, '_BoardIsSetUp')

    cpu.Upgrader.PreRunChecks()
    cpu._BoardIsSetUp('any-board').AndReturn(False)

    self.mox.ReplayAll()

    # Running with a package not set up should exit with code!=0
    self._PrepareArgv("--board=any-board", "--to-csv=/dev/null", "any-package")
    self._AssertCPUMain(cpu, expect_zero=False)
    self.mox.VerifyAll()

    # Verify that an error message was printed.
    (stdout, stderr) = self._RetrieveCapturedOutput()
    self._StopCapturingOutput()
    self._AssertOutputEndsInError(stdout)

  def testFlowStatusReportTwoBoards(self):
    """Test main flow for two-board status report."""
    self._StartCapturingOutput()

    self.mox.StubOutWithMock(cpu.Upgrader, 'PreRunChecks')
    self.mox.StubOutWithMock(cpu, '_BoardIsSetUp')
    self.mox.StubOutWithMock(cpu.Upgrader, 'PrepareToRun')
    self.mox.StubOutWithMock(cpu.Upgrader, 'RunBoard')
    self.mox.StubOutWithMock(cpu.Upgrader, 'RunCompleted')
    self.mox.StubOutWithMock(cpu.Upgrader, 'WriteTableFiles')

    cpu.Upgrader.PreRunChecks()
    cpu._BoardIsSetUp('board1').AndReturn(True)
    cpu._BoardIsSetUp('board2').AndReturn(True)
    cpu.Upgrader.PrepareToRun()
    cpu.Upgrader.RunBoard('board1')
    cpu.Upgrader.RunBoard('board2')
    cpu.Upgrader.RunCompleted()
    cpu.Upgrader.WriteTableFiles(csv=None)

    self.mox.ReplayAll()

    self._PrepareArgv("--board=board1:board2", "any-package")
    self._AssertCPUMain(cpu, expect_zero=True)
    self.mox.VerifyAll()

    self._StopCapturingOutput()

  def testFlowUpgradeOneBoard(self):
    """Test main flow for basic one-board upgrade."""
    self._StartCapturingOutput()

    self.mox.StubOutWithMock(cpu.Upgrader, 'PreRunChecks')
    self.mox.StubOutWithMock(cpu, '_BoardIsSetUp')
    self.mox.StubOutWithMock(cpu.Upgrader, 'PrepareToRun')
    self.mox.StubOutWithMock(cpu.Upgrader, 'RunBoard')
    self.mox.StubOutWithMock(cpu.Upgrader, 'RunCompleted')
    self.mox.StubOutWithMock(cpu.Upgrader, 'WriteTableFiles')

    cpu.Upgrader.PreRunChecks()
    cpu._BoardIsSetUp('any-board').AndReturn(True)
    cpu.Upgrader.PrepareToRun()
    cpu.Upgrader.RunBoard('any-board')
    cpu.Upgrader.RunCompleted()
    cpu.Upgrader.WriteTableFiles(csv=None)

    self.mox.ReplayAll()

    self._PrepareArgv("--upgrade", "--board=any-board", "any-package")
    self._AssertCPUMain(cpu, expect_zero=True)
    self.mox.VerifyAll()

    self._StopCapturingOutput()

  def testFlowUpgradeTwoBoards(self):
    """Test main flow for two-board upgrade."""
    self._StartCapturingOutput()

    self.mox.StubOutWithMock(cpu.Upgrader, 'PreRunChecks')
    self.mox.StubOutWithMock(cpu, '_BoardIsSetUp')
    self.mox.StubOutWithMock(cpu.Upgrader, 'PrepareToRun')
    self.mox.StubOutWithMock(cpu.Upgrader, 'RunBoard')
    self.mox.StubOutWithMock(cpu.Upgrader, 'RunCompleted')
    self.mox.StubOutWithMock(cpu.Upgrader, 'WriteTableFiles')

    cpu.Upgrader.PreRunChecks()
    cpu._BoardIsSetUp('board1').AndReturn(True)
    cpu._BoardIsSetUp('board2').AndReturn(True)
    cpu.Upgrader.PrepareToRun()
    cpu.Upgrader.RunBoard('board1')
    cpu.Upgrader.RunBoard('board2')
    cpu.Upgrader.RunCompleted()
    cpu.Upgrader.WriteTableFiles(csv='/dev/null')

    self.mox.ReplayAll()

    self._PrepareArgv("--upgrade", "--board=board1:board2",
                      "--to-csv=/dev/null", "any-package")
    self._AssertCPUMain(cpu, expect_zero=True)
    self.mox.VerifyAll()

    self._StopCapturingOutput()

  def testFlowUpgradeTwoBoardsAndHost(self):
    """Test main flow for two-board and host upgrade."""
    self._StartCapturingOutput()

    self.mox.StubOutWithMock(cpu.Upgrader, 'PreRunChecks')
    self.mox.StubOutWithMock(cpu, '_BoardIsSetUp')
    self.mox.StubOutWithMock(cpu.Upgrader, 'PrepareToRun')
    self.mox.StubOutWithMock(cpu.Upgrader, 'RunBoard')
    self.mox.StubOutWithMock(cpu.Upgrader, 'RunCompleted')
    self.mox.StubOutWithMock(cpu.Upgrader, 'WriteTableFiles')

    cpu.Upgrader.PreRunChecks()
    cpu._BoardIsSetUp('board1').AndReturn(True)
    cpu._BoardIsSetUp('board2').AndReturn(True)
    cpu.Upgrader.PrepareToRun()
    cpu.Upgrader.RunBoard(cpu.Upgrader.HOST_BOARD)
    cpu.Upgrader.RunBoard('board1')
    cpu.Upgrader.RunBoard('board2')
    cpu.Upgrader.RunCompleted()
    cpu.Upgrader.WriteTableFiles(csv='/dev/null')

    self.mox.ReplayAll()

    self._PrepareArgv("--upgrade", "--host", "--board=board1:host:board2",
                      "--to-csv=/dev/null", "any-package")
    self._AssertCPUMain(cpu, expect_zero=True)
    self.mox.VerifyAll()

    self._StopCapturingOutput()

if __name__ == '__main__':
  unittest.main()
