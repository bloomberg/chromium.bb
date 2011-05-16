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

import cros_portage_upgrade as cpu
import parallel_emerge
import portage.tests.resolver.ResolverPlayground as respgnd

# Configuration for generating a temporary valid ebuild hierarchy.
# TODO(mtennant): Wrap this mechanism to create multiple overlays.
EBUILDS = {
  "dev-libs/A-1": {"RDEPEND" : "dev-libs/B"},
  "dev-libs/A-2": {"RDEPEND" : "dev-libs/B"},
  "dev-libs/B-1": {"RDEPEND" : "dev-libs/C"},
  "dev-libs/B-2": {"RDEPEND" : "dev-libs/C"},
  "dev-libs/C-1": {},
  "dev-libs/C-2": {},
  "dev-libs/D-1": {},
  "dev-libs/D-2": {},
  "dev-libs/E-2": {},
  "dev-libs/E-3": {},

  "chromeos-base/flimflam-0.0.1-r228": {
    "EAPI" : "2",
    "SLOT" : "0",
    "KEYWORDS" : "amd64 x86 arm",
    "RDEPEND" : ">=dev-libs/D-2",
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

  "Virtual/libusb-0"         : {
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

INSTALLED = {
  "dev-libs/A-1": {"RDEPEND" : "dev-libs/B"},
  "dev-libs/B-1": {"RDEPEND" : "dev-libs/C"},
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
    "dev-libs/B-2" : "buildtime",
    "dev-libs/C-2" : "buildtime",
    "chromeos-base/libchrome-57098-r4" : "buildtime",
    "chromeos-base/flimflam-0.0.1-r228" : "buildtime"
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
                             'dev-libs/B-2',
                             'chromeos-base/flimflam-0.0.1-r228',
                             'dev-libs/D-2',
                             'dev-libs/C-2',
                             'chromeos-base/libchrome-57098-r4',
                             'dev-libs/E-3',
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

####################
### UpgraderTest ###
####################

class UpgraderTest(mox.MoxTestBase):
  """Test the Upgrader class from cros_portage_upgrade."""

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def _MockUpgrader(self, board='test_board', package='test_package',
                    verbose=False, rdeps=None, srcroot=None,
                    stable_repo=None, upstream_repo=None, csv_file=None):
    """Set up a mocked Upgrader object with the given args."""
    upgrader = self.mox.CreateMock(cpu.Upgrader)

    (options, args) = self._MockUpgraderOptions(board=board, package=package,
                                                verbose=verbose, rdeps=rdeps)

    upgrader._options = options
    upgrader._args = args
    upgrader._stable_repo = stable_repo
    upgrader._upstream_repo = upstream_repo
    upgrader._csv_file = csv_file

    return upgrader

  def _MockUpgraderOptions(self, board='test_board', package='test_package',
                           srcroot=None, upstream=None,
                           verbose=False, rdeps=None):
    """Mock optparse.Values for use with Upgrader, and create args list.

    Returns tuple with (options, args)."""

    if not srcroot:
      srcroot = '%s/trunk/src' % os.environ['HOME']

    options = self.mox.CreateMock(optparse.Values)
    options.board = board
    options.verbose = verbose
    options.rdeps = rdeps
    options.srcroot = srcroot
    options.upstream = upstream
    args = [package]

    return (options, args)

  def _SetUpEmerge(self):
    """Prepare the temporary ebuild playground and emerge variables.

    This leverages test code in existing Portage modules to create an ebuild
    hierarchy.  This can be a little slow."""

    # TODO(mtennant): Support multiple overlays?  This essentially
    # creates just a default overlay.
    self._playground = respgnd.ResolverPlayground(ebuilds=EBUILDS,
                                                  installed=INSTALLED)

    # Set all envvars needed by emerge, since --board is being skipped.
    eroot = self._playground.eroot
    if eroot[-1:] == '/':
      eroot = eroot[:-1]
    os.environ["PORTAGE_CONFIGROOT"] = eroot
    os.environ["PORTAGE_SYSROOT"] = eroot
    os.environ["SYSROOT"] = eroot
    os.environ.setdefault("CHROMEOS_ROOT", "%s/trunk" % os.environ["HOME"])
    os.environ["PORTDIR"] = "%s/usr/portage" % eroot

  def _TearDownEmerge(self):
    """Delete the temporary ebuild playground files."""
    try:
      self._playground.cleanup()
    except AttributeError:
      pass

  def _GetParallelEmergeArgv(self, mocked_upgrader):
    return cpu.Upgrader._GenParallelEmergeArgv(mocked_upgrader)

  #
  # _GetCurrentVersions testing
  #

  def _TestGetCurrentVersions(self, pkg):
    """Test the behavior of the Upgrader._GetCurrentVersions method.

    This basically confirms that it uses the parallel_emerge module to
    assemble the expected dependency graph."""
    mocked_upgrader = self._MockUpgrader(board=None, package=pkg, verbose=False)
    pm_argv = self._GetParallelEmergeArgv(mocked_upgrader)
    self._SetUpEmerge()

    # Add test-specific mocks/stubs.
    # TODO(mtennant): Stubbing this method may be overkill for this unit test
    self.mox.StubOutWithMock(cpu.Upgrader, '_GetPreOrderDepGraph')

    # Replay script
    verifier = _GenDepsGraphVerifier(pkg)
    mocked_upgrader._GenParallelEmergeArgv().AndReturn(pm_argv)
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
  # _GetPreOrderDepGraph testing
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
  # _SplitEBuildPath testing
  #

  def _TestSplitEBuildPath(self, ebuild_path, golden_result):
    """Test the behavior of the Upgrader._SplitEBuildPath method."""
    mocked_upgrader = self._MockUpgrader()

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._SplitEBuildPath(mocked_upgrader,
                                           ebuild_path)
    self.assertEquals(result, golden_result)
    self.mox.VerifyAll()

  def testSplitEBuildPath1(self):
    return self._TestSplitEBuildPath('/foo/bar/portage/dev-libs/A/A-2.ebuild',
                                     ('portage', 'dev-libs', 'A', 'A-2'))

  def testSplitEBuildPath2(self):
    return self._TestSplitEBuildPath('/foo/ooo/ccc/ppp/ppp-1.2.3-r123.ebuild',
                                     ('ooo', 'ccc', 'ppp', 'ppp-1.2.3-r123'))


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

  #
  # _UpgradePackages testing
  #
  # TODO(mtennant): Implement this.  It will require some cleverness.

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

    # Verify that a message containing "ERROR: " was printed
    (stdout, stderr) = self._RetrieveCapturedOutput()
    self._StopCapturingOutput()
    self.assertTrue("ERROR:" in stderr)

  def testMissingPackage(self):
    """Test that running without a package argument exits with an error."""
    self._PrepareArgv("--board=any-board")

    # Capture stdout/stderr so it can be verified later
    self._StartCapturingOutput()

    # Running without a package should exit with code!=0
    try:
      cpu.main()
    except exceptions.SystemExit, e:
      self.assertNotEquals(e.args[0], 0)

    # Verify that a message containing "ERROR: " was printed
    (stdout, stderr) = self._RetrieveCapturedOutput()
    self._StopCapturingOutput()
    self.assertTrue("ERROR:" in stderr)

  def testUpgraderRun(self):
    """Verify that running main method launches Upgrader.Run"""
    self.mox.StubOutWithMock(cpu.Upgrader, 'Run')
    cpu.Upgrader.Run()
    self.mox.ReplayAll()

    self._PrepareArgv("--board=any-board", "any-package")
    cpu.main()
    self.mox.VerifyAll()

if __name__ == '__main__':
  unittest.main()
