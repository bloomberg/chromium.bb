#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_portage_upgrade.py."""

import cStringIO
import exceptions
import filecmp
import optparse
import os
import re
import shutil
import sys
import tempfile
import unittest

import mox

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
import chromite.lib.cros_build_lib as cros_lib
import chromite.lib.upgrade_table as utable
import cros_portage_upgrade as cpu
import parallel_emerge
import portage.package.ebuild.config as portcfg
import portage.tests.resolver.ResolverPlayground as respgnd

# pylint: disable=W0212,R0904
# Regex to find the character sequence to turn text red (used for errors).
ERROR_PREFIX = re.compile('^\033\[1;31m')

DEFAULT_PORTDIR = '/usr/portage'

# Configuration for generating a temporary valid ebuild hierarchy.
# ResolverPlayground sets up a default profile with ARCH=x86, so
# other architectures are irrelevant for now.
DEFAULT_ARCH = 'x86'
EBUILDS = {
  "dev-libs/A-1": {"RDEPEND": "dev-libs/B"},
  "dev-libs/A-2": {"RDEPEND": "dev-libs/B"},
  "dev-libs/B-1": {"RDEPEND": "dev-libs/C"},
  "dev-libs/B-2": {"RDEPEND": "dev-libs/C"},
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
    "EAPI": "2",
    "SLOT": "0",
    "KEYWORDS": "amd64 x86 arm",
    "RDEPEND": ">=dev-libs/D-2",
    },
  "chromeos-base/flimflam-0.0.2-r123": {
    "EAPI": "2",
    "SLOT": "0",
    "KEYWORDS": "~amd64 ~x86 ~arm",
    "RDEPEND": ">=dev-libs/D-3",
    },
  "chromeos-base/libchrome-57098-r4": {
    "EAPI": "2",
    "SLOT": "0",
    "KEYWORDS": "amd64 x86 arm",
    "RDEPEND": ">=dev-libs/E-2",
    },
  "chromeos-base/libcros-1": {
    "EAPI": "2",
    "SLOT": "0",
    "KEYWORDS": "amd64 x86 arm",
    "RDEPEND": "dev-libs/B dev-libs/C chromeos-base/flimflam",
    "DEPEND":
    "dev-libs/B dev-libs/C chromeos-base/flimflam chromeos-base/libchrome",
    },

  "virtual/libusb-0"         : {
    "EAPI": "2", "SLOT": "0",
    "RDEPEND":
    "|| ( >=dev-libs/libusb-0.1.12-r1:0 dev-libs/libusb-compat " +
    ">=sys-freebsd/freebsd-lib-8.0[usb] )"},
  "virtual/libusb-1"         : {
    "EAPI":"2", "SLOT": "1",
    "RDEPEND": ">=dev-libs/libusb-1.0.4:1"},
  "dev-libs/libusb-0.1.13"   : {},
  "dev-libs/libusb-1.0.5"    : {"SLOT":"1"},
  "dev-libs/libusb-compat-1" : {},
  "sys-freebsd/freebsd-lib-8": {"IUSE": "+usb"},

  "sys-fs/udev-164"          : {"EAPI": "1", "RDEPEND": "virtual/libusb:0"},

  "virtual/jre-1.5.0"        : {
    "SLOT": "1.5",
    "RDEPEND": "|| ( =dev-java/sun-jre-bin-1.5.0* =virtual/jdk-1.5.0* )"},
  "virtual/jre-1.5.0-r1"     : {
    "SLOT": "1.5",
    "RDEPEND": "|| ( =dev-java/sun-jre-bin-1.5.0* =virtual/jdk-1.5.0* )"},
  "virtual/jre-1.6.0"        : {
    "SLOT": "1.6",
    "RDEPEND": "|| ( =dev-java/sun-jre-bin-1.6.0* =virtual/jdk-1.6.0* )"},
  "virtual/jre-1.6.0-r1"     : {
    "SLOT": "1.6",
    "RDEPEND": "|| ( =dev-java/sun-jre-bin-1.6.0* =virtual/jdk-1.6.0* )"},
  "virtual/jdk-1.5.0"        : {
    "SLOT": "1.5",
    "RDEPEND": "|| ( =dev-java/sun-jdk-1.5.0* dev-java/gcj-jdk )"},
  "virtual/jdk-1.5.0-r1"     : {
    "SLOT" : "1.5",
    "RDEPEND": "|| ( =dev-java/sun-jdk-1.5.0* dev-java/gcj-jdk )"},
  "virtual/jdk-1.6.0"        : {
    "SLOT": "1.6",
    "RDEPEND": "|| ( =dev-java/icedtea-6* =dev-java/sun-jdk-1.6.0* )"},
  "virtual/jdk-1.6.0-r1"     : {
    "SLOT": "1.6",
    "RDEPEND": "|| ( =dev-java/icedtea-6* =dev-java/sun-jdk-1.6.0* )"},
  "dev-java/gcj-jdk-4.5"     : {},
  "dev-java/gcj-jdk-4.5-r1"  : {},
  "dev-java/icedtea-6.1"     : {},
  "dev-java/icedtea-6.1-r1"  : {},
  "dev-java/sun-jdk-1.5"     : {"SLOT": "1.5"},
  "dev-java/sun-jdk-1.6"     : {"SLOT": "1.6"},
  "dev-java/sun-jre-bin-1.5" : {"SLOT": "1.5"},
  "dev-java/sun-jre-bin-1.6" : {"SLOT": "1.6"},

  "dev-java/ant-core-1.8"   : {"DEPEND" : ">=virtual/jdk-1.4"},
  "dev-db/hsqldb-1.8"       : {"RDEPEND": ">=virtual/jre-1.6"},
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
    "SLOT": "1.5",
    "RDEPEND": "|| ( =virtual/jdk-1.5.0* =dev-java/sun-jre-bin-1.5.0* )"},
  "virtual/jre-1.6.0"       : {
    "SLOT": "1.6",
    "RDEPEND": "|| ( =virtual/jdk-1.6.0* =dev-java/sun-jre-bin-1.6.0* )"},
  "virtual/jdk-1.5.0"       : {
    "SLOT": "1.5",
    "RDEPEND": "|| ( =dev-java/sun-jdk-1.5.0* dev-java/gcj-jdk )"},
  "virtual/jdk-1.6.0"       : {
    "SLOT": "1.6",
    "RDEPEND": "|| ( =dev-java/icedtea-6* =dev-java/sun-jdk-1.6.0* )"},
  "dev-java/gcj-jdk-4.5"    : {},
  "dev-java/icedtea-6.1"    : {},

  "virtual/libusb-0"        : {
    "EAPI": "2", "SLOT": "0",
    "RDEPEND":
    "|| ( >=dev-libs/libusb-0.1.12-r1:0 dev-libs/libusb-compat " +
    ">=sys-freebsd/freebsd-lib-8.0[usb] )"},
  }

# For verifying dependency graph results
GOLDEN_DEP_GRAPHS = {
  "dev-libs/A-2": { "needs": { "dev-libs/B-2": "runtime" },
                     "action": "merge" },
  "dev-libs/B-2": { "needs": { "dev-libs/C-2": "runtime" } },
  "dev-libs/C-2": { "needs": { } },
  "dev-libs/D-3": { "needs": { } },
  # TODO(mtennant): Bug in parallel_emerge deps graph makes blocker show up for
  # E-3, rather than in just E-2 where it belongs. See crosbug.com/22190.
  # To repeat bug, swap the commented status of next two lines.
  #"dev-libs/E-3": { "needs": { } },
  "dev-libs/E-3": { "needs": { "dev-libs/D-3": "blocker" } },
  "chromeos-base/libcros-1": { "needs": {
    "dev-libs/B-2": "runtime/buildtime",
    "dev-libs/C-2": "runtime/buildtime",
    "chromeos-base/libchrome-57098-r4": "buildtime",
    "chromeos-base/flimflam-0.0.1-r228": "runtime/buildtime"
    } },
  "chromeos-base/flimflam-0.0.1-r228": { "needs": {
    "dev-libs/D-3": "runtime"
    } },
  "chromeos-base/libchrome-57098-r4": { "needs": {
    "dev-libs/E-3": "runtime"
    } },
  }

# For verifying dependency set results
GOLDEN_DEP_SETS = {
  "dev-libs/A": set(['dev-libs/A-2', 'dev-libs/B-2', 'dev-libs/C-2']),
  "dev-libs/B": set(['dev-libs/B-2', 'dev-libs/C-2']),
  "dev-libs/C": set(['dev-libs/C-2']),
  "dev-libs/D": set(['dev-libs/D-3']),
  "virtual/libusb": set(['virtual/libusb-1', 'dev-libs/libusb-1.0.5']),
  "chromeos-base/libcros": set(['chromeos-base/libcros-1',
                                'dev-libs/B-2',
                                'chromeos-base/libchrome-57098-r4',
                                'dev-libs/E-3',
                                'chromeos-base/flimflam-0.0.1-r228',
                                'dev-libs/D-3',
                                'dev-libs/C-2',
                                ])
  }


def _GetGoldenDepsSet(pkg):
  """Retrieve the golden dependency set for |pkg| from GOLDEN_DEP_SETS."""
  return GOLDEN_DEP_SETS.get(pkg, None)

def _VerifyDepsGraph(deps_graph, pkgs):
  for pkg in pkgs:
    if not _VerifyDepsGraphOnePkg(deps_graph, pkg):
      return False

  return True

def _VerifyDepsGraphOnePkg(deps_graph, pkg):
  """Verfication function for Mox to validate deps graph for |pkg|."""

  if deps_graph is None:
    print("Error: no dependency graph passed into _GetPreOrderDepGraph")
    return False

  if type(deps_graph) != dict:
    print("Error: dependency graph is expected to be a dict.  Instead:\n%r" %
          deps_graph)
    return False

  validated = True

  golden_deps_set = _GetGoldenDepsSet(pkg)
  if golden_deps_set == None:
    print("Error: golden dependency list not configured for %s package" % pkg)
    validated = False

  # Verify dependencies, by comparing them to GOLDEN_DEP_GRAPHS
  for p in deps_graph:
    golden_pkg_info = None
    try:
      golden_pkg_info = GOLDEN_DEP_GRAPHS[p]
    except KeyError:
      print("Error: golden dependency graph not configured for %s package" % p)
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


def _GenDepsGraphVerifier(pkgs):
  """Generate a graph verification function for the given package."""
  return lambda deps_graph: _VerifyDepsGraph(deps_graph, pkgs)


def _IsErrorLine(line):
  """Return True if |line| has prefix associated with error output."""
  return ERROR_PREFIX.search(line)


class RunCommandResult():
  """Class to simulate result of cros_lib.RunCommand."""
  __slots__ = ['returncode', 'output']

  def __init__(self, returncode, output):
    self.returncode = returncode
    self.output = output


class CpuTestBase(mox.MoxTestBase):
  """Base class for all test classes in this file."""

  __slots__ = [
    'playground',
    'playground_envvars',
    ]

  def setUp(self):
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

  def _SetUpPlayground(self, ebuilds=EBUILDS, installed=INSTALLED, world=WORLD,
                       active=True):
    """Prepare the temporary ebuild playground (ResolverPlayground).

    This leverages test code in existing Portage modules to create an ebuild
    hierarchy.  This can be a little slow.

    |ebuilds| is a list of hashes representing ebuild files in
    a portdir.
    |installed| is a list of hashes representing ebuilds files
    already installed.
    |world| is a list of lines to simulate in the world file.
    |active| True means that os.environ variables should be set
    to point to the created playground, such that Portage tools
    (such as emerge) can be run now using the playground as the active
    PORTDIR.  Also saves the playground as self._playground. If |active|
    is False, then no os.environ variables are set and playground is
    not saved (only returned).

    Returns tuple (playground, envvars).
    """

    # TODO(mtennant): Support multiple overlays?  This essentially
    # creates just a default overlay.
    # Also note that ResolverPlayground assumes ARCH=x86 for the
    # default profile it creates.
    playground = respgnd.ResolverPlayground(ebuilds=ebuilds,
                                            installed=installed,
                                            world=world)

    # Set all envvars needed by parallel_emerge, since parallel_emerge
    # normally does that when --board is given.
    eroot = self._GetPlaygroundRoot(playground)
    portdir = self._GetPlaygroundPortdir(playground)
    envvars = {'PORTAGE_CONFIGROOT': eroot,
               'ROOT': eroot,
               'PORTDIR': portdir,
               }

    if active:
      for envvar in envvars:
        os.environ[envvar] = envvars[envvar]

      self.playground = playground
      self.playground_envvars = envvars

    return (playground, envvars)

  def _GetPlaygroundRoot(self, playground=None):
    """Get the temp dir playground is using as ROOT."""
    if playground is None:
      playground = self.playground

    eroot = playground.eroot
    if eroot[-1:] == '/':
      eroot = eroot[:-1]
    return eroot

  def _GetPlaygroundPortdir(self, playground=None):
    """Get the temp portdir without playground."""
    if playground is None:
      playground = self.playground

    eroot = self._GetPlaygroundRoot(playground)
    portdir = "%s%s" % (eroot, DEFAULT_PORTDIR)
    return portdir

  def _TearDownPlayground(self, playground=None):
    """Delete the temporary ebuild playground files."""
    if playground:
      playground.cleanup()
    elif self.playground:
      self.playground.cleanup()
      self.playground = None
      self.playground_envvars = None

  def _MockUpgrader(self, cmdargs=None, **kwargs):
    """Set up a mocked Upgrader object with the given args."""
    upgrader_slot_defaults = {
      '_curr_arch':   DEFAULT_ARCH,
      '_curr_board':  'some_board',
      '_unstable_ok': False,
      '_verbose':     False,
    }

    upgrader = self.mox.CreateMock(cpu.Upgrader)

    # Initialize each attribute with default value.
    for slot in cpu.Upgrader.__slots__:
      value = upgrader_slot_defaults.get(slot, None)
      upgrader.__setattr__(slot, value)

    # Initialize with command line if given.
    if cmdargs is not None:
      def ParseCmdArgs(cmdargs):
        """Returns (options, args) tuple."""
        parser = cpu._CreateOptParser()
        return parser.parse_args(args=cmdargs)

      (options, args) = ParseCmdArgs(cmdargs)
      cpu.Upgrader.__init__(upgrader, options, args)

    # Override Upgrader attributes if requested.
    for slot in cpu.Upgrader.__slots__:
      value = None
      if slot in kwargs:
        upgrader.__setattr__(slot, kwargs[slot])

    return upgrader

########################
### CopyUpstreamTest ###
########################

class CopyUpstreamTest(CpuTestBase):
  """Test Upgrader._CopyUpstreamPackage and _CopyUpstreamEclass"""

  # This is a hack until crosbug.com/21965 is completed and upstreamed
  # to Portage.  Insert eclass simulation into tree.
  def _AddEclassToPlayground(self, eclass, lines=None,
                             ebuilds=None, missing=False):
    """Hack to insert an eclass into the playground source.

    |eclass| Name of eclass to create (without .eclass suffix).  Will be
    created as an empty file unless |lines| is specified.
    |lines| Lines of text to put into created eclass, if given.
    |ebuilds| List of ebuilds to put inherit line into.  Should be path
    to ebuild from playground portdir.
    |missing| If True, do not actually create the eclass file.  Only makes
    sense if |ebuilds| is non-empty, presumably to test inherit failure.

    Return full path to the eclass file, whether it was created or not.
    """
    portdir = self._GetPlaygroundPortdir()
    eclass_path = os.path.join(portdir, 'eclass', '%s.eclass' % eclass)

    # Create the eclass file
    fd = open(eclass_path, 'w')
    if lines:
      for line in lines:
        fd.write(line + '\n')
    fd.close()

    # Insert the inherit line into the ebuild file, if requested.
    if ebuilds:
      for ebuild in ebuilds:
        ebuild_path = os.path.join(portdir, ebuild)

        fd = open(ebuild_path, 'r')
        text = fd.read()
        fd.close()

        def repl(match):
          return match.group(1) + '\ninherit ' + eclass
        text = re.sub(r'(EAPI.*)', repl, text)

        fd = open(ebuild_path, 'w')
        fd.write(text)
        fd.close()

        # Remove the Manifest file
        os.remove(os.path.join(os.path.dirname(ebuild_path), 'Manifest'))

        # Recreate the Manifests using the ebuild utility.
        cmd = ['ebuild', ebuild_path, 'manifest']
        cros_lib.RunCommand(cmd, print_cmd=False, redirect_stdout=True,
                            combine_stdout_stderr=True)

    # If requested, remove the eclass.
    if missing:
      os.remove(eclass_path)

    return eclass_path

  #
  # _IdentifyNeededEclass
  #

  def _TestIdentifyNeededEclass(self, cpv, ebuild, eclass, create_eclass):
    """Test Upgrader._IdentifyNeededEclass"""

    self._SetUpPlayground()
    portdir = self._GetPlaygroundPortdir()
    mocked_upgrader = self._MockUpgrader(cmdargs=[],
                                         _stable_repo=portdir,
                                         )
    self._AddEclassToPlayground(eclass,
                                ebuilds=[ebuild],
                                missing=not create_eclass)

    # Add test-specific mocks/stubs

    # Replay script
    envvars = cpu.Upgrader._GenPortageEnvvars(mocked_upgrader,
                                              mocked_upgrader._curr_arch,
                                              unstable_ok=True)
    mocked_upgrader._GenPortageEnvvars(mocked_upgrader._curr_arch,
                                       unstable_ok=True,
                                       ).AndReturn(envvars)
    mocked_upgrader._GetBoardCmd('equery').AndReturn('equery')
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._IdentifyNeededEclass(mocked_upgrader, cpv)
    self.mox.VerifyAll()

    # Cleanup
    self._TearDownPlayground()

    return result

  def testIdentifyNeededEclassMissing(self):
    result = self._TestIdentifyNeededEclass('dev-libs/A-2',
                                            'dev-libs/A/A-2.ebuild',
                                            'inheritme',
                                            False)
    self.assertEquals('inheritme.eclass', result)

  def testIdentifyNeededEclassOK(self):
    result = self._TestIdentifyNeededEclass('dev-libs/A-2',
                                            'dev-libs/A/A-2.ebuild',
                                            'inheritme',
                                            True)
    self.assertTrue(result is None)

  #
  # _CopyUpstreamEclass
  #

  def _TestCopyUpstreamEclass(self, eclass, do_copy, local_copy_identical=None):
    """Test Upgrader._CopyUpstreamEclass"""

    self._SetUpPlayground()
    upstream_portdir = self._GetPlaygroundPortdir()
    portage_stable = tempfile.mkdtemp()
    mocked_upgrader = self._MockUpgrader(_curr_board=None,
                                         _upstream_repo=upstream_portdir,
                                         _stable_repo=portage_stable,
                                         )

    eclass_subpath = os.path.join('eclass', eclass + '.eclass')
    eclass_path = os.path.join(portage_stable, eclass_subpath)
    upstream_eclass_path = None
    if do_copy or local_copy_identical:
      lines = ['#Dummy eclass', '#Hi']
      upstream_eclass_path = self._AddEclassToPlayground(eclass,
                                                         lines=lines)
    if local_copy_identical:
      # Make it look like identical eclass already exists in portage-stable.
      os.makedirs(os.path.dirname(eclass_path))
      shutil.copy2(upstream_eclass_path, eclass_path)
    elif local_copy_identical is not None:
      # Make local copy some other gibberish.
      os.makedirs(os.path.dirname(eclass_path))
      fd = open(eclass_path, 'w')
      fd.write("garblety gook")
      fd.close()

    # Add test-specific mocks/stubs

    # Replay script
    if do_copy:
      mocked_upgrader._RunGit(mocked_upgrader._stable_repo,
                              'add ' + eclass_subpath)
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._CopyUpstreamEclass(mocked_upgrader,
                                              eclass + '.eclass')
    self.mox.VerifyAll()

    if do_copy:
      self.assertTrue(result)
      # Verify that eclass has been copied into portage-stable.
      self.assertTrue(os.path.exists(eclass_path))
      # Verify that eclass contents are correct.
      self.assertTrue(filecmp.cmp(upstream_eclass_path, eclass_path))

    else:
      self.assertFalse(result)

    # Cleanup
    self._TearDownPlayground()
    shutil.rmtree(portage_stable)

  def testCopyUpstreamEclassCopyBecauseMissing(self):
    self._TestCopyUpstreamEclass('inheritme',
                                 do_copy=True)

  def testCopyUpstreamEclassCopyBecauseDifferent(self):
    self._TestCopyUpstreamEclass('inheritme',
                                 do_copy=True,
                                 local_copy_identical=False)

  def testCopyUpstreamEclassNoCopyBecauseIdentical(self):
    self._TestCopyUpstreamEclass('inheritme',
                                 do_copy=False,
                                 local_copy_identical=True)

  def testCopyUpstreamEclassNoCopyBecauseUpstreamMissing(self):
    self.assertRaises(RuntimeError,
                      self._TestCopyUpstreamEclass,
                      'inheritme',
                      do_copy=False)

  #
  # _CopyUpstreamPackage
  #

  def _TestCopyUpstreamPackage(self, catpkg, verrev, success,
                               existing_files, extra_upstream_files):
    """Test Upgrader._CopyUpstreamPackage"""

    upstream_cpv = '%s-%s' % (catpkg, verrev)
    ebuild = '%s-%s.ebuild' % (catpkg.split('/')[-1], verrev)

    self._SetUpPlayground()
    upstream_portdir = self._GetPlaygroundPortdir()

    # Simulate extra files in upsteam package dir.
    if extra_upstream_files:
      pkg_dir = os.path.join(upstream_portdir, catpkg)
      if os.path.exists(pkg_dir):
        for extra_file in extra_upstream_files:
          open(os.path.join(pkg_dir, extra_file), 'w')

    # Prepare dummy portage-stable dir, with extra previously
    # existing files simulated if requested.
    portage_stable = tempfile.mkdtemp()
    if existing_files:
      pkg_dir = os.path.join(portage_stable, catpkg)
      os.makedirs(pkg_dir)
      for existing_file in existing_files:
        open(os.path.join(pkg_dir, existing_file), 'w')


    mocked_upgrader = self._MockUpgrader(_curr_board=None,
                                         _upstream_repo=upstream_portdir,
                                         _stable_repo=portage_stable,
                                         )

    # Add test-specific mocks/stubs

    # Replay script
    if success:
      pkgdir = os.path.join(mocked_upgrader._stable_repo, catpkg)
      for existing_file in existing_files:
        mocked_upgrader._RunGit(pkgdir, 'rm -rf ' + existing_file,
                                redirect_stdout=True).InAnyOrder()
      mocked_upgrader._RunGit(mocked_upgrader._stable_repo,
                              'add ' + catpkg)
      mocked_upgrader._IdentifyNeededEclass(upstream_cpv).AndReturn(None)
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._CopyUpstreamPackage(mocked_upgrader, upstream_cpv)
    self.mox.VerifyAll()

    if success:
      self.assertEquals(result, upstream_cpv)
      # Verify that ebuild has been copied into portage-stable.
      ebuild_path = os.path.join(portage_stable, catpkg, ebuild)
      self.assertTrue(os.path.exists(ebuild_path))
      # Verify that any extra files upstream are also copied
      for extra_file in extra_upstream_files:
        file_path = os.path.join(portage_stable, catpkg, extra_file)
        self.assertTrue(os.path.exists(file_path))
    else:
      self.assertTrue(result is None)

    # Cleanup
    self._TearDownPlayground()
    shutil.rmtree(portage_stable)

  def testCopyUpstreamPackageEmptyStable(self):
    existing_files = []
    extra_upstream_files = []
    result = self._TestCopyUpstreamPackage('dev-libs/D', '2', True,
                                           existing_files,
                                           extra_upstream_files)

  def testCopyUpstreamPackageClutteredStable(self):
    existing_files = ['foo', 'bar', 'foobar.ebuild', 'D-1.ebuild']
    extra_upstream_files = []
    result = self._TestCopyUpstreamPackage('dev-libs/D', '2', True,
                                           existing_files,
                                           extra_upstream_files)

  def testCopyUpstreamPackageVersionNotAvailable(self):
    """Should fail, dev-libs/D version 5 does not exist 'upstream'"""
    existing_files = []
    extra_upstream_files = []
    self.assertRaises(RuntimeError,
                      self._TestCopyUpstreamPackage,
                      'dev-libs/D', '5', False,
                      existing_files,
                      extra_upstream_files)

  def testCopyUpstreamPackagePackageNotAvailable(self):
    """Should fail, a-b-c/D does not exist 'upstream' in any version"""
    existing_files = []
    extra_upstream_files = []
    self.assertRaises(RuntimeError,
                      self._TestCopyUpstreamPackage,
                      'a-b-c/D', '5', False,
                      existing_files,
                      extra_upstream_files)

  def testCopyUpstreamPackageExtraUpstreamFiles(self):
    existing_files = ['foo', 'bar']
    extra_upstream_files = ['keepme', 'andme']
    result = self._TestCopyUpstreamPackage('dev-libs/F', '2-r1', True,
                                           existing_files,
                                           extra_upstream_files)

##################################
### GetPackageUpgradeStateTest ###
##################################

class GetPackageUpgradeStateTest(CpuTestBase):
  """Test Upgrader._GetPackageUpgradeState"""

  def _TestGetPackageUpgradeState(self, pinfo,
                                  exists_upstream,
                                  ):

    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)

    # Add test-specific mocks/stubs

    # Replay script
    mocked_upgrader._FindUpstreamCPV(pinfo['cpv'], unstable_ok=True,
                                     ).AndReturn(exists_upstream)
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._GetPackageUpgradeState(mocked_upgrader, pinfo)
    self.mox.VerifyAll()

    return result

  def testGetPackageUpgradeStateLocalOnly(self):
    pinfo = {'cpv': 'foo/bar-2',
             'overlay': 'chromiumos-overlay',
             'cpv_cmp_upstream': None,
             'latest_upstream_cpv': None,
             }
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=False)
    self.assertEquals(result, utable.UpgradeTable.STATE_LOCAL_ONLY)

  def testGetPackageUpgradeStateUnknown(self):
    pinfo = {'cpv': 'foo/bar-2',
             'overlay': 'portage',
             'cpv_cmp_upstream': None,
             'latest_upstream_cpv': None,
             }
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=False)
    self.assertEquals(result, utable.UpgradeTable.STATE_UNKNOWN)

  def testGetPackageUpgradeStateUpgradeAndDuplicated(self):
    pinfo = {'cpv': 'foo/bar-2',
             'overlay': 'chromiumos-overlay',
             'cpv_cmp_upstream': 1, # outdated
             'latest_upstream_cpv': 'not important',
             }
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=True)
    self.assertEquals(result,
                      utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_DUPLICATED)

  def testGetPackageUpgradeStateUpgradeAndPatched(self):
    pinfo = {'cpv': 'foo/bar-2',
             'overlay': 'chromiumos-overlay',
             'cpv_cmp_upstream': 1, # outdated
             'latest_upstream_cpv': 'not important',
             }
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=False)
    self.assertEquals(result,
                      utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_PATCHED)

  def testGetPackageUpgradeStateUpgrade(self):
    pinfo = {'cpv': 'foo/bar-2',
             'overlay': 'portage-stable',
             'cpv_cmp_upstream': 1, # outdated
             'latest_upstream_cpv': 'not important',
             }
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=False)
    self.assertEquals(result, utable.UpgradeTable.STATE_NEEDS_UPGRADE)

  def testGetPackageUpgradeStateDuplicated(self):
    pinfo = {'cpv': 'foo/bar-2',
             'overlay': 'chromiumos-overlay',
             'cpv_cmp_upstream': 0, # current
             'latest_upstream_cpv': 'not important',
             }
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=True)
    self.assertEquals(result, utable.UpgradeTable.STATE_DUPLICATED)

  def testGetPackageUpgradeStatePatched(self):
    pinfo = {'cpv': 'foo/bar-2',
             'overlay': 'chromiumos-overlay',
             'cpv_cmp_upstream': 0, # current
             'latest_upstream_cpv': 'not important',
             }
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=False)
    self.assertEquals(result, utable.UpgradeTable.STATE_PATCHED)

  def testGetPackageUpgradeStateCurrent(self):
    pinfo = {'cpv': 'foo/bar-2',
             'overlay': 'portage-stable',
             'cpv_cmp_upstream': 0, # current
             'latest_upstream_cpv': 'not important',
             }
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=False)
    self.assertEquals(result, utable.UpgradeTable.STATE_CURRENT)

######################
### EmergeableTest ###
######################

class EmergeableTest(CpuTestBase):
  """Test Upgrader._AreEmergeable."""

  def _TestAreEmergeable(self, cpvlist, expect,
                         unstable_ok=False, debug=False,
                         world=WORLD):
    """Test the Upgrader._AreEmergeable method.

    |cpvlist| and |unstable_ok| are passed to _AreEmergeable.
    |expect| is boolean, expected return value of _AreEmergeable
    |debug| requests that emerge output in _AreEmergeable be shown.
    |world| is list of lines to override default world contents.
    """

    cmdargs = ['--upgrade'] + cpvlist
    if unstable_ok:
      cmdargs = ['--unstable-ok'] + cmdargs
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)
    self._SetUpPlayground(world=world)

    # Add test-specific mocks/stubs

    # Replay script
    envvars = cpu.Upgrader._GenPortageEnvvars(mocked_upgrader,
                                              mocked_upgrader._curr_arch,
                                              unstable_ok)
    mocked_upgrader._GenPortageEnvvars(mocked_upgrader._curr_arch,
                                       unstable_ok).AndReturn(envvars)
    mocked_upgrader._GetBoardCmd('emerge').AndReturn('emerge')
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._AreEmergeable(mocked_upgrader, cpvlist, unstable_ok)
    self.mox.VerifyAll()

    (code, cmd, output) = result
    if debug or code != expect:
      print("\nTest ended with success==%r (expected==%r)" % (code, expect))
      print("Emerge output:\n%s" % output)

    self.assertEquals(code, expect)

    self._TearDownPlayground()

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
    return self._TestAreEmergeable(cpvlist, True, unstable_ok=True)

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

#####################
### CPVUtilTest ###
#####################

class CPVUtilTest(CpuTestBase):
  """Test various CPV utilities in Upgrader"""

  def _TestCmpCpv(self, cpv1, cpv2):
    """Test Upgrader._CmpCpv"""
    # Add test-specific mocks/stubs

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._CmpCpv(cpv1, cpv2)
    self.mox.VerifyAll()

    return result

  def testCmpCpv(self):
    # cpvs to compare
    equal = [('foo/bar-1', 'foo/bar-1'),
             ('a-b-c/x-y-z-1.2.3-r1', 'a-b-c/x-y-z-1.2.3-r1'),
             ('foo/bar-1', 'foo/bar-1-r0'),
             (None, None),
             ]
    for (cpv1, cpv2) in equal:
      self.assertEqual(0, self._TestCmpCpv(cpv1, cpv2))

    lessthan = [(None, 'foo/bar-1'),
                ('foo/bar-1', 'foo/bar-2'),
                ('foo/bar-1', 'foo/bar-1-r1'),
                ('foo/bar-1-r1', 'foo/bar-1-r2'),
                ('foo/bar-1.2.3', 'foo/bar-1.2.4'),
                ('foo/bar-5a', 'foo/bar-5b'),
                ]
    for (cpv1, cpv2) in lessthan:
      self.assertTrue(self._TestCmpCpv(cpv1, cpv2) < 0)
      self.assertTrue(self._TestCmpCpv(cpv2, cpv1) > 0)

    not_comparable = [('foo/bar-1', 'bar/foo-1'),
                      ]
    for (cpv1, cpv2) in not_comparable:
      self.assertEquals(None, self._TestCmpCpv(cpv1, cpv2))

  def _TestGetCatPkgFromCpv(self, cpv):
    """Test Upgrader._GetCatPkgFromCpv"""
    # Add test-specific mocks/stubs

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._GetCatPkgFromCpv(cpv)
    self.mox.VerifyAll()

    return result

  def testGetCatPkgFromCpv(self):
    # (input, output) tuples
    data = [('foo/bar-1', 'foo/bar'),
            ('a-b-c/x-y-z-1', 'a-b-c/x-y-z'),
            ('a-b-c/x-y-z-1.2.3-r3', 'a-b-c/x-y-z'),
            ('bar-1', 'bar'),
            ('bar', None),
            ]

    for (cpv, catpn) in data:
      result = self._TestGetCatPkgFromCpv(cpv)
      self.assertEquals(catpn, result)

  def _TestGetVerRevFromCpv(self, cpv):
    """Test Upgrader._GetVerRevFromCpv"""
    # Add test-specific mocks/stubs

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._GetVerRevFromCpv(cpv)
    self.mox.VerifyAll()

    return result

  def testGetVerRevFromCpv(self):
    # (input, output) tuples
    data = [('foo/bar-1', '1'),
            ('a-b-c/x-y-z-1', '1'),
            ('a-b-c/x-y-z-1.2.3-r3', '1.2.3-r3'),
            ('foo/bar-3.222-r0', '3.222'),
            ('bar-1', '1'),
            ('bar', None),
            ]

    for (cpv, verrev) in data:
      result = self._TestGetVerRevFromCpv(cpv)
      self.assertEquals(verrev, result)

#########################
### PortageStableTest ###
#########################

class PortageStableTest(CpuTestBase):
  """Test Upgrader methods _SaveStatusOnStableRepo, _CheckStableRepoOnBranch"""

  STATUS_MIX = {'path1/file1': 'M',
                'path1/path2/file2': 'A',
                'a/b/.x/y~': 'D',
                'x/y/z': 'R',
                'foo/bar': 'C',
                '/bar/foo': 'U',
                'unknown/file': '??',
                }
  STATUS_UNKNOWN = {'foo/bar': '??',
                    'a/b/c': '??',
                    }
  STATUS_EMPTY = {}

  #
  # _CheckStableRepoOnBranch
  #

  def _TestCheckStableRepoOnBranch(self, run_result, expect_err):
    """Test Upgrader._CheckStableRepoOnBranch"""
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)

    # Add test-specific mocks/stubs

    # Replay script
    mocked_upgrader._RunGit(mocked_upgrader._stable_repo,
                            'branch', redirect_stdout=True,
                            ).AndReturn(run_result)
    self.mox.ReplayAll()

    # Verify
    try:
      cpu.Upgrader._CheckStableRepoOnBranch(mocked_upgrader)
      self.assertFalse(expect_err, "Expected RuntimeError, but none raised.")
    except RuntimeError as ex:
      self.assertTrue(expect_err, "Unexpected RuntimeError: %s" % str(ex))
    self.mox.VerifyAll()

  def testCheckStableRepoOnBranchNoBranch(self):
    """Should fail due to 'git branch' saying 'no branch'"""
    output = '* (no branch)\n  somebranch\n  otherbranch\n'
    run_result = RunCommandResult(returncode=0, output=output)
    self._TestCheckStableRepoOnBranch(run_result, True)

  def testCheckStableRepoOnBranchOK1(self):
    """Should pass as 'git branch' indicates a branch"""
    output = '* somebranch\n  otherbranch\n'
    run_result = RunCommandResult(returncode=0, output=output)
    self._TestCheckStableRepoOnBranch(run_result, False)

  def testCheckStableRepoOnBranchOK2(self):
    """Should pass as 'git branch' indicates a branch"""
    output = '  somebranch\n* otherbranch\n'
    run_result = RunCommandResult(returncode=0, output=output)
    self._TestCheckStableRepoOnBranch(run_result, False)

  def testCheckStableRepoOnBranchFail(self):
    """Should fail as 'git branch' failed"""
    output = 'does not matter'
    run_result = RunCommandResult(returncode=1, output=output)
    self._TestCheckStableRepoOnBranch(run_result, True)

  #
  # _SaveStatusOnStableRepo
  #

  def _TestSaveStatusOnStableRepo(self, run_result):
    """Test Upgrader._SaveStatusOnStableRepo"""
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)

    # Add test-specific mocks/stubs

    # Replay script
    mocked_upgrader._RunGit(mocked_upgrader._stable_repo,
                            'status -s', redirect_stdout=True,
                            ).AndReturn(run_result)
    self.mox.ReplayAll()

    # Verify
    cpu.Upgrader._SaveStatusOnStableRepo(mocked_upgrader)
    self.mox.VerifyAll()

    self.assertFalse(mocked_upgrader._stable_repo_stashed)
    return mocked_upgrader._stable_repo_status

  def testSaveStatusOnStableRepoFailed(self):
    """Test case where 'git status -s' fails, should raise RuntimeError"""
    run_result = RunCommandResult(returncode=1,
                                  output=None)

    self.assertRaises(RuntimeError,
                      self._TestSaveStatusOnStableRepo,
                      run_result)

  def testSaveStatusOnStableRepoAllKinds(self):
    """Test where 'git status -s' returns all status kinds"""
    status_lines = ['%2s %s' % (v, k) for (k, v) in self.STATUS_MIX.items()]
    status_output = '\n'.join(status_lines)
    run_result = RunCommandResult(returncode=0,
                                  output=status_output)
    status = self._TestSaveStatusOnStableRepo(run_result)
    self.assertEqual(status, self.STATUS_MIX)

  def testSaveStatusOnStableRepoEmpty(self):
    """Test empty response from 'git status -s'"""
    run_result = RunCommandResult(returncode=0,
                                  output='')
    status = self._TestSaveStatusOnStableRepo(run_result)
    self.assertEqual(status, {})

  #
  # _AnyChangesStaged
  #

  def _TestAnyChangesStaged(self, status_dict):
    """Test Upgrader._AnyChangesStaged"""
    mocked_upgrader = self._MockUpgrader(_stable_repo_status=status_dict)

    # Add test-specific mocks/stubs

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._AnyChangesStaged(mocked_upgrader)
    self.mox.VerifyAll()

    return result

  def testAnyChangesStagedMix(self):
    """Should return True"""
    self.assertTrue(self._TestAnyChangesStaged(self.STATUS_MIX),
                    "Failed to notice files with changed status.")

  def testAnyChangesStagedUnknown(self):
    """Should return False, only files with '??' status"""
    self.assertFalse(self._TestAnyChangesStaged(self.STATUS_UNKNOWN),
                    "Should not consider files with '??' status.")

  def testAnyChangesStagedEmpty(self):
    """Should return False, no file statuses"""
    self.assertFalse(self._TestAnyChangesStaged(self.STATUS_EMPTY),
                    "No files should mean no changes staged.")

  #
  # _StashChanges
  #

  def testStashChanges(self):
    """Test Upgrader._StashChanges"""
    mocked_upgrader = self._MockUpgrader(cmdargs=[],
                                         _stable_repo_stashed=False)
    self.assertFalse(mocked_upgrader._stable_repo_stashed)

    # Add test-specific mocks/stubs

    # Replay script
    mocked_upgrader._RunGit(mocked_upgrader._stable_repo,
                            'stash save',
                            redirect_stdout=True,
                            combine_stdout_stderr=True)
    self.mox.ReplayAll()

    # Verify
    cpu.Upgrader._StashChanges(mocked_upgrader)
    self.mox.VerifyAll()

    self.assertTrue(mocked_upgrader._stable_repo_stashed)

  #
  # _UnstashAnyChanges
  #

  def _TestUnstashAnyChanges(self, stashed):
    """Test Upgrader._UnstashAnyChanges"""
    mocked_upgrader = self._MockUpgrader(cmdargs=[],
                                         _stable_repo_stashed=stashed)
    self.assertEqual(stashed, mocked_upgrader._stable_repo_stashed)

    # Add test-specific mocks/stubs

    # Replay script
    if stashed:
      mocked_upgrader._RunGit(mocked_upgrader._stable_repo,
                              'stash pop',
                              redirect_stdout=True,
                              combine_stdout_stderr=True)
    self.mox.ReplayAll()

    # Verify
    cpu.Upgrader._UnstashAnyChanges(mocked_upgrader)
    self.mox.VerifyAll()

    self.assertFalse(mocked_upgrader._stable_repo_stashed)

  def testUnstashAnyChanges(self):
    self._TestUnstashAnyChanges(True)
    self._TestUnstashAnyChanges(False)

  #
  # _DropAnyStashedChanges
  #

  def _TestDropAnyStashedChanges(self, stashed):
    """Test Upgrader._DropAnyStashedChanges"""
    mocked_upgrader = self._MockUpgrader(cmdargs=[],
                                         _stable_repo_stashed=stashed)
    self.assertEqual(stashed, mocked_upgrader._stable_repo_stashed)

    # Add test-specific mocks/stubs

    # Replay script
    if stashed:
      mocked_upgrader._RunGit(mocked_upgrader._stable_repo,
                              'stash drop',
                              redirect_stdout=True,
                              combine_stdout_stderr=True)
    self.mox.ReplayAll()

    # Verify
    cpu.Upgrader._DropAnyStashedChanges(mocked_upgrader)
    self.mox.VerifyAll()

    self.assertFalse(mocked_upgrader._stable_repo_stashed)

  def testDropAnyStashedChanges(self):
    self._TestDropAnyStashedChanges(True)
    self._TestDropAnyStashedChanges(False)

###################
### UtilityTest ###
###################

class UtilityTest(CpuTestBase):
  """Test several Upgrader methods.

  Test these Upgrader methods: _SplitEBuildPath, _GenPortageEnvvars"""

  #
  # _GetPkgKeywordsFile
  #

  def testGetPkgKeywordsFile(self):
    """Test Upgrader._GetPkgKeywordsFile."""
    root = '/foo/bar'
    cmdargs = ['--srcroot=%s' % root]
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)

    # Add test-specific mocks/stubs

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._GetPkgKeywordsFile(mocked_upgrader)
    self.mox.VerifyAll()

    overlay = 'chromiumos-overlay'
    path = 'profiles/default/linux'
    golden_result = ('%s/third_party/%s/%s/package.keywords' %
                     (root, overlay, path))
    self.assertEquals(result, golden_result)

  #
  # _IsInUpgradeMode
  #

  def _TestIsInUpgradeMode(self, cmdargs):
    """Test Upgrader._IsInUpgradeMode.  Pretty simple."""
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)

    # Add test-specific mocks/stubs

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._IsInUpgradeMode(mocked_upgrader)
    self.mox.VerifyAll()

    return result

  def testIsInUpgradeModeNoOpts(self):
    """Should not be in upgrade mode with no options."""
    result = self._TestIsInUpgradeMode([])
    self.assertFalse(result)

  def testIsInUpgradeModeUpgrade(self):
    """Should be in upgrade mode with --upgrade."""
    result = self._TestIsInUpgradeMode(['--upgrade'])
    self.assertTrue(result)

  def testIsInUpgradeModeUpgradeDeep(self):
    """Should be in upgrade mode with --upgrade-deep."""
    result = self._TestIsInUpgradeMode(['--upgrade-deep'])
    self.assertTrue(result)

  #
  # _GetBoardCmd
  #

  def _TestGetBoardCmd(self, cmd, board):
    """Test Upgrader._GetBoardCmd."""
    mocked_upgrader = self._MockUpgrader(_curr_board=board)

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
    mocked_upgrader = self._MockUpgrader()

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
    mocked_upgrader = self._MockUpgrader()

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

class TreeInspectTest(CpuTestBase):
  """Test Upgrader methods: _FindCurrentCPV, _FindUpstreamCPV"""

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

    self._SetUpPlayground()
    eroot = self._GetPlaygroundRoot()
    mocked_upgrader = self._MockUpgrader(_curr_board=None,
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

    self._TearDownPlayground()

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

    mocked_upgrader = self._MockUpgrader(_curr_board=None)
    self._SetUpPlayground()
    eroot = self._GetPlaygroundRoot()

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

    self._TearDownPlayground()

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

class RunBoardTest(CpuTestBase):
  """Test Upgrader.RunBoard,PrepareToRun,RunCompleted."""

  def testRunCompletedSpecified(self):
    cmdargs = ['--upstream=/some/dir']
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _emptydir='empty-dir',
                                         _curr_board=None)

    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(os, 'rmdir')

    # Replay script
    os.rmdir('empty-dir')
    self.mox.ReplayAll()

    # Verify
    cpu.Upgrader.RunCompleted(mocked_upgrader)
    self.mox.VerifyAll()

  def testRunCompletedRemoveCache(self):
    cmdargs = ['--no-upstream-cache']
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _emptydir='empty-dir',
                                         _curr_board=None)

    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(shutil, 'rmtree')
    self.mox.StubOutWithMock(os, 'rmdir')
    self.mox.StubOutWithMock(os.path, 'exists')

    # Replay script
    shutil.rmtree(mocked_upgrader._upstream_repo)
    os.path.exists(mocked_upgrader._upstream_repo + '-README').AndReturn(False)
    os.rmdir('empty-dir')
    self.mox.ReplayAll()

    # Verify
    cpu.Upgrader.RunCompleted(mocked_upgrader)
    self.mox.VerifyAll()

  def testRunCompletedKeepCache(self):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _emptydir='empty-dir',
                                         _curr_board=None)

    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(os, 'rmdir')

    # Replay script
    os.rmdir('empty-dir')
    self.mox.ReplayAll()

    # Verify
    cpu.Upgrader.RunCompleted(mocked_upgrader)
    self.mox.VerifyAll()

  def testPrepareToRunUpstreamSpecified(self):
    cmdargs = ['--upstream=/some/dir']
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)

    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(tempfile, 'mkdtemp')

    # Replay script
    tempfile.mkdtemp()
    self.mox.ReplayAll()

    # Verify
    cpu.Upgrader.PrepareToRun(mocked_upgrader)
    self.mox.VerifyAll()

  def testPrepareToRunUpstreamRepoExists(self):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)

    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(tempfile, 'mkdtemp')
    self.mox.StubOutWithMock(os.path, 'exists')

    # Replay script
    os.path.exists(mocked_upgrader._upstream_repo).AndReturn(True)
    mocked_upgrader._RunGit(mocked_upgrader._upstream_repo,
                            'remote update')
    mocked_upgrader._RunGit(mocked_upgrader._upstream_repo,
                            'checkout origin/gentoo',
                            redirect_stdout=True,
                            combine_stdout_stderr=True)
    tempfile.mkdtemp()
    self.mox.ReplayAll()

    # Verify
    cpu.Upgrader.PrepareToRun(mocked_upgrader)
    self.mox.VerifyAll()

  def testPrepareToRunUpstreamRepoNew(self):
    tmpdir = tempfile.mkdtemp()
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _upstream_repo=tmpdir,
                                         _curr_board=None)

    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(tempfile, 'mkdtemp')
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(os.path, 'dirname')
    self.mox.StubOutWithMock(os.path, 'basename')

    # Replay script
    os.path.exists(mocked_upgrader._upstream_repo).AndReturn(False)
    root = os.path.dirname(mocked_upgrader._upstream_repo).AndReturn('root')
    name = os.path.basename(mocked_upgrader._upstream_repo).AndReturn('name')
    mocked_upgrader._RunGit(root,
                            'clone %s %s' %
                            (cpu.Upgrader.PORTAGE_GIT_URL, name))
    mocked_upgrader._RunGit(mocked_upgrader._upstream_repo,
                            'checkout origin/gentoo',
                            redirect_stdout=True,
                            combine_stdout_stderr=True)
    tempfile.mkdtemp()
    self.mox.ReplayAll()

    # Verify
    cpu.Upgrader.PrepareToRun(mocked_upgrader)
    self.mox.VerifyAll()

    self.mox.UnsetStubs()

    readme_path = tmpdir + '-README'
    self.assertTrue(os.path.exists(readme_path))
    os.remove(readme_path)
    os.rmdir(tmpdir)

  def _TestRunBoard(self, infolist, upgrade=False, staged_changes=False):
    """Test Upgrader.RunBoard."""

    targetlist = [info['user_arg'] for info in infolist]
    local_infolist = [info for info in infolist if 'cpv' in info]
    upstream_only_infolist = [info for info in infolist if 'cpv' not in info]

    cmdargs = targetlist
    if upgrade:
      cmdargs = ['--upgrade'] + cmdargs
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
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
    if upgrade:
      mocked_upgrader._FinalizeUpstreamInfolist(infolist).AndReturn([])
    else:
      mocked_upgrader._GetCurrentVersions(infolist).AndReturn(infolist)
      mocked_upgrader._FinalizeLocalInfolist(infolist).AndReturn([])

      if upgrade_mode:
        mocked_upgrader._FinalizeUpstreamInfolist(
          upstream_only_infolist).AndReturn([])

    mocked_upgrader._UnstashAnyChanges()
    mocked_upgrader._UpgradePackages([])

    mocked_upgrader._DropAnyStashedChanges()

    self.mox.ReplayAll()

    # Verify
    cpu.Upgrader.RunBoard(mocked_upgrader, board)
    self.mox.VerifyAll()

  def testRunBoard1(self):
    target_infolist = [{'user_arg':     'dev-libs/A',
                        'cpv':          'dev-libs/A-1',
                        'upstream_cpv': 'dev-libs/A-2',
                        },
                       ]
    return self._TestRunBoard(target_infolist)

  def testRunBoard2(self):
    target_infolist = [{'user_arg':     'dev-libs/A',
                        'cpv':          'dev-libs/A-1',
                        'upstream_cpv': 'dev-libs/A-2',
                        },
                       ]
    return self._TestRunBoard(target_infolist, upgrade=True)

  def testRunBoard3(self):
    target_infolist = [{'user_arg':     'dev-libs/A',
                        'cpv':          'dev-libs/A-1',
                        'upstream_cpv': 'dev-libs/A-2',
                        },
                       ]
    return self._TestRunBoard(target_infolist, upgrade=True,
                              staged_changes=True)

  def testRunBoardUpstreamOnlyStatusMode(self):
    """Status mode with package that is only upstream should error."""

    infolist = [{'user_arg':     'dev-libs/M',
                 'cpv':          None,
                 'upstream_cpv': 'dev-libs/M-2',
                 },
                ]

    targetlist = [info['user_arg'] for info in infolist]
    local_infolist = []
    upstream_only_infolist = [info for info in infolist if 'cpv' not in info]

    mocked_upgrader = self._MockUpgrader(cmdargs=['dev-libs/M'],
                                         _curr_board=None)
    board = 'runboard_testboard'

    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(cpu.Upgrader, '_FindBoardArch')

    # Replay script
    mocked_upgrader._SaveStatusOnStableRepo().AndReturn(None)
    cpu.Upgrader._FindBoardArch(board).AndReturn('x86')
    upgrade_mode = cpu.Upgrader._IsInUpgradeMode(mocked_upgrader)
    mocked_upgrader._IsInUpgradeMode().AndReturn(upgrade_mode)
    mocked_upgrader._AnyChangesStaged().AndReturn(False)

    mocked_upgrader._ResolveAndVerifyArgs(targetlist,
                                          upgrade_mode).AndReturn(infolist)
    mocked_upgrader._DropAnyStashedChanges()
    self.mox.ReplayAll()

    # Verify
    self.assertRaises(RuntimeError,
                      cpu.Upgrader.RunBoard,
                      mocked_upgrader, board)
    self.mox.VerifyAll()


#############################
### GiveEmergeResultsTest ###
#############################

class GiveEmergeResultsTest(CpuTestBase):
  """Test Upgrader._GiveEmergeResults"""

  def _TestGiveEmergeResultsOK(self, infolist, ok):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)

    # Add test-specific mocks/stubs

    # Replay script
    mocked_upgrader._AreEmergeable(mox.IgnoreArg(),
                                   mocked_upgrader._unstable_ok,
                                   ).AndReturn((ok, None, None))
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._GiveEmergeResults(mocked_upgrader, infolist)
    self.mox.VerifyAll()

    return result

  def testGiveEmergeResultsUnmaskedOK(self):
    infolist = [{'upgraded_cpv': 'abc/def-4', 'upgraded_unmasked': True},
                {'upgraded_cpv': 'bcd/efg-8', 'upgraded_unmasked': True},
                ]
    self._TestGiveEmergeResultsOK(infolist, True)

  def testGiveEmergeResultsUnmaskedNotOK(self):
    infolist = [{'upgraded_cpv': 'abc/def-4', 'upgraded_unmasked': True},
                {'upgraded_cpv': 'bcd/efg-8', 'upgraded_unmasked': True},
                ]
    self.assertRaises(RuntimeError,
                      self._TestGiveEmergeResultsOK,
                      infolist, False)

  def _TestGiveEmergeResultsMasked(self, infolist, ok, masked_cpvs):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)

    # Add test-specific mocks/stubs

    # Replay script
    emergeable_tuple = (ok, 'some-cmd', 'some-output')
    mocked_upgrader._AreEmergeable(mox.IgnoreArg(),
                                   mocked_upgrader._unstable_ok,
                                   ).AndReturn(emergeable_tuple)
    if not ok:
      for cpv in masked_cpvs:
        mocked_upgrader._GiveMaskedError(cpv, 'some-output').InAnyOrder()
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._GiveEmergeResults(mocked_upgrader, infolist)
    self.mox.VerifyAll()

    return result

  def testGiveEmergeResultsMaskedOK(self):
    infolist = [{'upgraded_cpv': 'abc/def-4', 'upgraded_unmasked': False},
                {'upgraded_cpv': 'bcd/efg-8', 'upgraded_unmasked': False},
                ]
    masked_cpvs = ['abc/def-4', 'bcd/efg-8']
    self.assertRaises(RuntimeError,
                      self._TestGiveEmergeResultsMasked,
                      infolist, True, masked_cpvs)

  def testGiveEmergeResultsMaskedNotOK(self):
    infolist = [{'upgraded_cpv': 'abc/def-4', 'upgraded_unmasked': False},
                {'upgraded_cpv': 'bcd/efg-8', 'upgraded_unmasked': False},
                ]
    masked_cpvs = ['abc/def-4', 'bcd/efg-8']
    self.assertRaises(RuntimeError,
                      self._TestGiveEmergeResultsMasked,
                      infolist, False, masked_cpvs)


###############################
### CheckStagedUpgradesTest ###
###############################

class CheckStagedUpgradesTest(CpuTestBase):
  """Test Upgrader._CheckStagedUpgrades"""

  def testCheckStagedUpgradesTwoStaged(self):
    cmdargs = []

    ebuild1 = 'a/b/foo/bar/bar-1.ebuild'
    ebuild2 = 'x/y/bar/foo/foo-3.ebuild'
    repo_status = {ebuild1: 'A',
                   'a/b/foo/garbage': 'A',
                   ebuild2: 'A',
                   }

    infolist = [{'upgraded_cpv': 'foo/bar-1'},
                {'upgraded_cpv': 'bar/foo-3'},
                ]

    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _stable_repo_status=repo_status)

    # Add test-specific mocks/stubs

    # Replay script
    for ebuild in [ebuild1, ebuild2]:
      split = cpu.Upgrader._SplitEBuildPath(mocked_upgrader, ebuild)
      mocked_upgrader._SplitEBuildPath(ebuild).InAnyOrder().AndReturn(split)

    self.mox.ReplayAll()

    # Verify
    cpu.Upgrader._CheckStagedUpgrades(mocked_upgrader, infolist)
    self.mox.VerifyAll()

  def testCheckStagedUpgradesNoneStaged(self):
    cmdargs = []

    infolist = [{'upgraded_cpv': 'foo/bar-1'},
                {'upgraded_cpv': 'bar/foo-3'},
                ]

    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _stable_repo_status=None)

    # Add test-specific mocks/stubs

    # Replay script
    self.mox.ReplayAll()

    # Verify
    cpu.Upgrader._CheckStagedUpgrades(mocked_upgrader, infolist)
    self.mox.VerifyAll()

###########################
### UpgradePackagesTest ###
###########################

class UpgradePackagesTest(CpuTestBase):
  """Test Upgrader._UpgradePackages"""

  def _TestUpgradePackages(self, infolist, upgrade):
    cmdargs = []
    if upgrade:
      cmdargs.append('--upgrade')
    table = utable.UpgradeTable('some-arch')
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_table=table)

    # Add test-specific mocks/stubs

    # Replay script
    upgrades_this_run = False
    for pinfo in infolist:
      pkg_result = bool(pinfo['upgraded_cpv'])
      mocked_upgrader._UpgradePackage(pinfo).InAnyOrder('up'
                                                        ).AndReturn(pkg_result)
      if pkg_result:
        upgrades_this_run = True

    for pinfo in infolist:
      if pinfo['upgraded_cpv']:
        mocked_upgrader._VerifyPackageUpgrade(pinfo).InAnyOrder('ver')
      mocked_upgrader._PackageReport(pinfo).InAnyOrder('ver')

    if upgrades_this_run:
      mocked_upgrader._GiveEmergeResults(infolist)

    upgrade_mode = cpu.Upgrader._IsInUpgradeMode(mocked_upgrader)
    mocked_upgrader._IsInUpgradeMode().AndReturn(upgrade_mode)
    if upgrade_mode:
      mocked_upgrader._CheckStagedUpgrades(infolist)
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._UpgradePackages(mocked_upgrader, infolist)
    self.mox.VerifyAll()

  def testUpgradePackagesUpgradeModeWithUpgrades(self):
    infolist = [{'upgraded_cpv': 'abc/def-4'},
                {'upgraded_cpv': 'bcd/efg-8'},
                {'upgraded_cpv': None},
                {'upgraded_cpv': None}
                ]
    self._TestUpgradePackages(infolist, True)

  def testUpgradePackagesUpgradeModeNoUpgrades(self):
    infolist = [{'upgraded_cpv': None},
                {'upgraded_cpv': None},
                ]
    self._TestUpgradePackages(infolist, True)

  def testUpgradePackagesStatusModeNoUpgrades(self):
    infolist = [{'upgraded_cpv': None},
                {'upgraded_cpv': None},
                ]
    self._TestUpgradePackages(infolist, False)

##########################
### UpgradePackageTest ###
##########################

class UpgradePackageTest(CpuTestBase):
  """Test Upgrader._UpgradePackage"""

  def _TestUpgradePackage(self, pinfo, upstream_cpv, upstream_cmp,
                          stable_up, latest_up,
                          upgrade_requested, upgrade_staged,
                          unstable_ok, force):
    cmdargs = []
    if unstable_ok:
      cmdargs.append('--unstable-ok')
    if force:
      cmdargs.append('--force')
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)

    # Add test-specific mocks/stubs

    # Replay script
    mocked_upgrader._FindUpstreamCPV(pinfo['package']).AndReturn(stable_up)
    mocked_upgrader._FindUpstreamCPV(pinfo['package'],
                                     unstable_ok=True).AndReturn(latest_up)
    if upstream_cpv:
      mocked_upgrader._PkgUpgradeRequested(pinfo).AndReturn(upgrade_requested)
      if upgrade_requested:
        mocked_upgrader._PkgUpgradeStaged(upstream_cpv
                                          ).AndReturn(upgrade_staged)
        if (not upgrade_staged and
            (upstream_cmp > 0 or (upstream_cmp == 0 and force))):
          mocked_upgrader._CopyUpstreamPackage(upstream_cpv
                                               ).AndReturn(upstream_cpv)
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._UpgradePackage(mocked_upgrader, pinfo)
    self.mox.VerifyAll()

    if upstream_cpv:
      self.assertEquals(upstream_cpv, pinfo['upstream_cpv'])

      if upgrade_requested and (upstream_cpv != pinfo['cpv'] or force):
        self.assertEquals(upstream_cpv, pinfo['upgraded_cpv'])
      else:
        self.assertTrue(pinfo['upgraded_cpv'] is None)
    else:
      self.assertTrue(pinfo['upstream_cpv'] is None)
      self.assertTrue(pinfo['upgraded_cpv'] is None)
    self.assertEquals(stable_up, pinfo['stable_upstream_cpv'])
    self.assertEquals(latest_up, pinfo['latest_upstream_cpv'])

    return result

  # Dimensions to vary:
  # 1) Upgrade for this package requested or not
  # 2) Upgrade can be stable or not
  # 3) Specific version to upgrade to specified
  # 4) Upgrade already staged or not
  # 5) Upgrade needed or not (current)

  def testUpgradePackageOutdatedRequestedStable(self):
    pinfo = {'cpv': 'foo/bar-2',
             'package': 'foo/bar',
             'upstream_cpv': None,
             }
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-3',
                                      upstream_cmp=1, # outdated
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=False,
                                      unstable_ok=False,
                                      force=False,
                                      )
    self.assertTrue(result)

  def testUpgradePackageOutdatedRequestedUnstable(self):
    pinfo = {'cpv': 'foo/bar-2',
             'package': 'foo/bar',
             'upstream_cpv': None,
             }
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-5',
                                      upstream_cmp=1, # outdated
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=False,
                                      unstable_ok=True,
                                      force=False,
                                      )
    self.assertTrue(result)

  def testUpgradePackageOutdatedRequestedStableSpecified(self):
    pinfo = {'cpv': 'foo/bar-2',
             'package': 'foo/bar',
             'upstream_cpv': 'foo/bar-4',
             }
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-4',
                                      upstream_cmp=1, # outdated
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=False,
                                      unstable_ok=False, # not important
                                      force=False,
                                      )
    self.assertTrue(result)

  def testUpgradePackageCurrentRequestedStable(self):
    pinfo = {'cpv': 'foo/bar-3',
             'package': 'foo/bar',
             'upstream_cpv': None,
             }
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-3',
                                      upstream_cmp=0, # current
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=False,
                                      unstable_ok=False,
                                      force=False,
                                      )
    self.assertFalse(result)

  def testUpgradePackageCurrentRequestedStable(self):
    pinfo = {'cpv': 'foo/bar-3',
             'package': 'foo/bar',
             'upstream_cpv': None,
             }
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-3',
                                      upstream_cmp=0, # current
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=False,
                                      unstable_ok=False,
                                      force=False,
                                      )
    self.assertFalse(result)

  def testUpgradePackageCurrentRequestedStableForce(self):
    pinfo = {'cpv': 'foo/bar-3',
             'package': 'foo/bar',
             'upstream_cpv': 'foo/bar-3',
             }
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-3',
                                      upstream_cmp=0, # current
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=False,
                                      unstable_ok=False,
                                      force=True,
                                      )
    self.assertTrue(result)

  def testUpgradePackageOutdatedStable(self):
    pinfo = {'cpv': 'foo/bar-2',
             'package': 'foo/bar',
             'upstream_cpv': None,
             }
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-3',
                                      upstream_cmp=1, # outdated
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=False,
                                      upgrade_staged=False,
                                      unstable_ok=False,
                                      force=False,
                                      )
    self.assertFalse(result)

  def testUpgradePackageOutdatedRequestedStableStaged(self):
    pinfo = {'cpv': 'foo/bar-2',
             'package': 'foo/bar',
             'upstream_cpv': None,
             }
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-3',
                                      upstream_cmp=1, # outdated
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=True,
                                      unstable_ok=False,
                                      force=False,
                                      )
    self.assertTrue(result)

  def testUpgradePackageOutdatedRequestedUnstableStaged(self):
    pinfo = {'cpv': 'foo/bar-2',
             'package': 'foo/bar',
             'upstream_cpv': None,
             }
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-5',
                                      upstream_cmp=1, # outdated
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=True,
                                      unstable_ok=True,
                                      force=False,
                                      )
    self.assertTrue(result)

#########################
### VerifyPackageTest ###
#########################

class VerifyPackageTest(CpuTestBase):

  def _TestVerifyPackageUpgrade(self, pinfo, unmasked, stable):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)
    was_overwrite = pinfo['cpv_cmp_upstream'] == 0

    # Add test-specific mocks/stubs

    # Replay script
    mocked_upgrader._GetMaskBits(pinfo['upgraded_cpv']
                                 ).AndReturn((unmasked, stable))
    mocked_upgrader._VerifyEbuildOverlay(pinfo['upgraded_cpv'],
                                         'portage-stable',
                                         stable, was_overwrite)
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._VerifyPackageUpgrade(mocked_upgrader, pinfo)
    self.mox.VerifyAll()

    self.assertEquals(unmasked, pinfo['upgraded_unmasked'])
    self.assertEquals(stable, pinfo['upgraded_stable'])

  def testVerifyPackageUpgrade(self):
    pinfo = {'upgraded_cpv': 'foo/bar-3',
             }

    for cpv_cmp_upstream in (0, 1):
      pinfo['cpv_cmp_upstream'] = cpv_cmp_upstream
      self._TestVerifyPackageUpgrade(pinfo, True, True)
      self._TestVerifyPackageUpgrade(pinfo, True, False)
      self._TestVerifyPackageUpgrade(pinfo, False, True)
      self._TestVerifyPackageUpgrade(pinfo, False, False)

  def _TestVerifyEbuildOverlay(self, cpv, overlay, ebuild_path, was_overwrite):
    """Test Upgrader._VerifyEbuildOverlay"""
    stable_only = True # not important

    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_arch=DEFAULT_ARCH,
                                         )

    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')

    # Replay script
    envvars = cpu.Upgrader._GenPortageEnvvars(mocked_upgrader,
                                              mocked_upgrader._curr_arch,
                                              not stable_only)
    mocked_upgrader._GenPortageEnvvars(mocked_upgrader._curr_arch,
                                       not stable_only).AndReturn(envvars)
    mocked_upgrader._GetBoardCmd('equery').AndReturn('equery')
    run_result = RunCommandResult(returncode=0,
                                  output=ebuild_path)
    cros_lib.RunCommand(['equery', 'which', '--include-masked', cpv],
                        exit_code=True, error_ok=True,
                        extra_env=envvars, print_cmd=False,
                        redirect_stdout=True, combine_stdout_stderr=True,
                        ).AndReturn(run_result)
    split_ebuild = cpu.Upgrader._SplitEBuildPath(mocked_upgrader, ebuild_path)
    mocked_upgrader._SplitEBuildPath(ebuild_path).AndReturn(split_ebuild)
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._VerifyEbuildOverlay(mocked_upgrader, cpv,
                                               overlay, stable_only,
                                               was_overwrite)
    self.mox.VerifyAll()

  def testVerifyEbuildOverlayGood(self):
    cpv = 'foo/bar-2'
    overlay = 'some-overlay'
    good_path = '/some/path/%s/foo/bar/bar-2.ebuild' % overlay

    self._TestVerifyEbuildOverlay(cpv, overlay, good_path, False)

  def testVerifyEbuildOverlayEvilNonOverwrite(self):
    cpv = 'foo/bar-2'
    overlay = 'some-overlay'
    evil_path = '/some/path/spam/foo/bar/bar-2.ebuild'

    self.assertRaises(RuntimeError,
                      self._TestVerifyEbuildOverlay,
                      cpv, overlay, evil_path, False)

  def testVerifyEbuildOverlayEvilOverwrite(self):
    cpv = 'foo/bar-2'
    overlay = 'some-overlay'
    evil_path = '/some/path/spam/foo/bar/bar-2.ebuild'

    self.assertRaises(RuntimeError,
                      self._TestVerifyEbuildOverlay,
                      cpv, overlay, evil_path, True)

  def _TestGetMaskBits(self, cpv, output):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_arch=DEFAULT_ARCH,
                                         )

    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')

    # Replay script
    mocked_upgrader._GenPortageEnvvars(mocked_upgrader._curr_arch,
                                       False).AndReturn('envvars')
    mocked_upgrader._GetBoardCmd('equery').AndReturn('equery')
    run_result = RunCommandResult(returncode=0,
                                  output=output)
    cros_lib.RunCommand(['equery', '--no-pipe', 'list', '-op', cpv],
                        exit_code=True, error_ok=True,
                        extra_env='envvars', print_cmd=False,
                        redirect_stdout=True, combine_stdout_stderr=True,
                        ).AndReturn(run_result)
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._GetMaskBits(mocked_upgrader, cpv)
    self.mox.VerifyAll()

    return result

  def testGetMaskBitsUnmaskedStable(self):
    output = "[-P-] [  ] foo/bar-2.7.0:0"
    result = self._TestGetMaskBits('foo/bar-2.7.0', output)
    self.assertEquals(result, (True, True))

  def testGetMaskBitsUnmaskedUnstable(self):
    output = "[-P-] [ ~] foo/bar-2.7.3:0"
    result = self._TestGetMaskBits('foo/bar-2.7.3', output)
    self.assertEquals(result, (True, False))

  def testGetMaskBitsMaskedStable(self):
    output = "[-P-] [M ] foo/bar-2.7.4:0"
    result = self._TestGetMaskBits('foo/bar-2.7.4', output)
    self.assertEquals(result, (False, True))

  def testGetMaskBitsMaskedUnstable(self):
    output = "[-P-] [M~] foo/bar-2.7.4-r1:0"
    result = self._TestGetMaskBits('foo/bar-2.7.4-r1', output)
    self.assertEquals(result, (False, False))

##################
### CommitTest ###
##################

class CommitTest(CpuTestBase):
  """Test various commit-related Upgrader methods"""

  #
  # _ExtractUpgradedPkgs
  #

  def _TestExtractUpgradedPkgs(self, upgrade_lines):
    """Test Upgrader._ExtractUpgradedPkgs"""
    mocked_upgrader = self._MockUpgrader()

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._ExtractUpgradedPkgs(mocked_upgrader, upgrade_lines)
    self.mox.VerifyAll()

    return result

  #
  # _ExtractUpgradedPkgs
  #

  def _TestExtractUpgradedPkgs(self, upgrade_lines):
    """Test Upgrader._ExtractUpgradedPkgs"""
    mocked_upgrader = self._MockUpgrader()

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._ExtractUpgradedPkgs(mocked_upgrader, upgrade_lines)
    self.mox.VerifyAll()

    return result

  def testExtractUpgradedPkgs(self):
    upgrade_lines = ["Upgraded abc/efg to version 1.2.3 on amd64, arm, x86",
                     "Upgraded xyz/uvw to version 1.2.3 on amd64",
                     "Upgraded xyz/uvw to version 3.2.1 on arm, x86",
                     "Upgraded mno/pqr to version 12345 on x86",
                     ]
    result = self._TestExtractUpgradedPkgs(upgrade_lines)
    self.assertEquals(result, ['efg', 'pqr', 'uvw'])

  #
  # _AmendCommitMessage
  #

  def _TestAmendCommitMessage(self, new_upgrade_lines,
                              old_upgrade_lines, remaining_lines,
                              git_show):
    """Test Upgrader._AmendCommitMessage"""
    mocked_upgrader = self._MockUpgrader()

    gold_lines = new_upgrade_lines + old_upgrade_lines
    def all_lines_verifier(lines):
      return gold_lines == lines

    # Add test-specific mocks/stubs

    # Replay script
    git_result = RunCommandResult(returncode=0,
                                  output=git_show)
    mocked_upgrader._RunGit(mocked_upgrader._stable_repo,
                            mox.IgnoreArg(), redirect_stdout=True,
                            ).AndReturn(git_result)
    mocked_upgrader._CreateCommitMessage(mox.Func(all_lines_verifier),
                                         remaining_lines)
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._AmendCommitMessage(mocked_upgrader,
                                              new_upgrade_lines)
    self.mox.VerifyAll()

  def testOldAndNew(self):
    new_upgrade_lines = ["Upgraded abc/efg to version 1.2.3 on amd64, arm, x86",
                         "Upgraded mno/pqr to version 4.5-r1 on x86",
                         ]
    old_upgrade_lines = ["Upgraded xyz/uvw to version 3.2.1 on arm, x86",
                         "Upgraded mno/pqr to version 12345 on x86",
                         ]
    remaining_lines = ["Extraneous extra comments in commit body.",
                       "",
                       "BUG=chromium-os:12345",
                       "TEST=test everything",
                       "again and again",
                       ]
    git_show_output = ("__BEGIN BODY__\n"
                       + "\n".join(old_upgrade_lines) + "\n"
                       + "\n"
                       + "\n".join(remaining_lines) +
                       "__END BODY__\n"
                       "some other stuff\n"
                       "even more stuff\n"
                       "not important\n"
                       )
    self._TestAmendCommitMessage(new_upgrade_lines, old_upgrade_lines,
                                 remaining_lines, git_show_output)

  def testOldOnly(self):
    old_upgrade_lines = ["Upgraded xyz/uvw to version 3.2.1 on arm, x86",
                         "Upgraded mno/pqr to version 12345 on x86",
                         ]
    git_show_output = ("__BEGIN BODY__\n"
                       + "\n".join(old_upgrade_lines) + "\n"
                       "__END BODY__\n"
                       "some other stuff\n"
                       "even more stuff\n"
                       "not important\n"
                       )
    self._TestAmendCommitMessage([], old_upgrade_lines, [], git_show_output)

  def testNewOnly(self):
    new_upgrade_lines = ["Upgraded abc/efg to version 1.2.3 on amd64, arm, x86",
                         "Upgraded mno/pqr to version 4.5-r1 on x86",
                         ]
    git_show_output = ("some other stuff\n"
                       "even more stuff\n"
                       "not important\n"
                       )
    self._TestAmendCommitMessage(new_upgrade_lines, [], [], git_show_output)

  def testOldEditedAndNew(self):
    new_upgrade_lines = ["Upgraded abc/efg to version 1.2.3 on amd64, arm, x86",
                         "Upgraded mno/pqr to version 4.5-r1 on x86",
                         ]
    old_upgrade_lines = ["So I upgraded xyz/uvw to version 3.2.1 on arm, x86",
                         "Then I Upgraded mno/pqr to version 12345 on x86",
                         ]
    remaining_lines = ["Extraneous extra comments in commit body.",
                       "",
                       "BUG=chromium-os:12345",
                       "TEST=test everything",
                       "again and again",
                       ]
    git_show_output = ("__BEGIN BODY__\n"
                       + "\n".join(old_upgrade_lines) + "\n"
                       + "\n"
                       + "\n".join(remaining_lines) +
                       "__END BODY__\n"
                       "some other stuff\n"
                       "even more stuff\n"
                       "not important\n"
                       )

    # In this test, it should not recognize the existing old_upgrade_lines
    # as a previous commit message from this script.  So it should give a
    # warning and push those lines to the end (grouped with remaining_lines).
    remaining_lines = old_upgrade_lines + [''] + remaining_lines
    self._TestAmendCommitMessage(new_upgrade_lines, [],
                                 remaining_lines, git_show_output)

  #
  # _CreateCommitMessage
  #

  def _TestCreateCommitMessage(self, upgrade_lines):
    """Test Upgrader._CreateCommitMessage"""
    mocked_upgrader = self._MockUpgrader()

    # Add test-specific mocks/stubs

    # Replay script
    upgrade_pkgs = cpu.Upgrader._ExtractUpgradedPkgs(mocked_upgrader,
                                                     upgrade_lines)
    mocked_upgrader._ExtractUpgradedPkgs(upgrade_lines).AndReturn(upgrade_pkgs)
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._CreateCommitMessage(mocked_upgrader, upgrade_lines)
    self.mox.VerifyAll()

    self.assertTrue(result.startswith("Upgraded "))
    return result

  def testCreateCommitMessageOnePkg(self):
    upgrade_lines = ["Upgraded abc/efg to version 1.2.3 on amd64, arm, x86",
                     ]
    result = self._TestCreateCommitMessage(upgrade_lines)

    # Commit message should have:
    # -- Summary that mentions 'efg' and ends in "package" (singular)
    # -- Body corresponding to upgrade_lines
    # -- BUG= line (with space after '=' to invalidate it)
    # -- TEST= line (with space after '=' to invalidate it)
    body = r'\n'.join([re.sub("\s+", r'\s', line) for line in upgrade_lines])
    regexp = re.compile(r'''^Upgraded\s.*efg.*package\n       # Summary
                            ^\s*\n                            # Blank line
                            %s\n                              # Body
                            ^\s*\n                            # Blank line
                            ^BUG=\s.+\n                       # BUG line
                            ^TEST=\s                          # TEST line
                            ''' % body,
                        re.VERBOSE | re.MULTILINE)
    self.assertTrue(regexp.search(result))

  def testCreateCommitMessageThreePkgs(self):
    upgrade_lines = ["Upgraded abc/efg to version 1.2.3 on amd64, arm, x86",
                     "Upgraded xyz/uvw to version 1.2.3 on amd64",
                     "Upgraded xyz/uvw to version 3.2.1 on arm, x86",
                     "Upgraded mno/pqr to version 12345 on x86",
                     ]
    result = self._TestCreateCommitMessage(upgrade_lines)

    # Commit message should have:
    # -- Summary that mentions 'efg, pqr, uvw' and ends in "packages" (plural)
    # -- Body corresponding to upgrade_lines
    # -- BUG= line (with space after '=' to invalidate it)
    # -- TEST= line (with space after '=' to invalidate it)
    body = r'\n'.join([re.sub("\s+", r'\s', line) for line in upgrade_lines])
    regexp = re.compile(r'''^Upgraded\s.*efg,\spqr,\suvw.*packages\n # Summary
                            ^\s*\n                            # Blank line
                            %s\n                              # Body
                            ^\s*\n                            # Blank line
                            ^BUG=\s.+\n                       # BUG line
                            ^TEST=\s                          # TEST line
                            ''' % body,
                        re.VERBOSE | re.MULTILINE)
    self.assertTrue(regexp.search(result))

  def testCreateCommitMessageTenPkgs(self):
    upgrade_lines = ["Upgraded abc/efg to version 1.2.3 on amd64, arm, x86",
                     "Upgraded bcd/fgh to version 1.2.3 on amd64",
                     "Upgraded cde/ghi to version 3.2.1 on arm, x86",
                     "Upgraded def/hij to version 12345 on x86",
                     "Upgraded efg/ijk to version 1.2.3 on amd64",
                     "Upgraded fgh/jkl to version 3.2.1 on arm, x86",
                     "Upgraded ghi/klm to version 12345 on x86",
                     "Upgraded hij/lmn to version 1.2.3 on amd64",
                     "Upgraded ijk/mno to version 3.2.1 on arm, x86",
                     "Upgraded jkl/nop to version 12345 on x86",
                     ]
    result = self._TestCreateCommitMessage(upgrade_lines)

    # Commit message should have:
    # -- Summary that mentions '10' and ends in "packages" (plural)
    # -- Body corresponding to upgrade_lines
    # -- BUG= line (with space after '=' to invalidate it)
    # -- TEST= line (with space after '=' to invalidate it)
    body = r'\n'.join([re.sub("\s+", r'\s', line) for line in upgrade_lines])
    regexp = re.compile(r'''^Upgraded\s.*10.*\spackages\n     # Summary
                            ^\s*\n                            # Blank line
                            %s\n                              # Body
                            ^\s*\n                            # Blank line
                            ^BUG=\s.+\n                       # BUG line
                            ^TEST=\s                          # TEST line
                            ''' % body,
                        re.VERBOSE | re.MULTILINE)
    self.assertTrue(regexp.search(result))

##############################
### GetCurrentVersionsTest ###
##############################

class GetCurrentVersionsTest(CpuTestBase):
  """Test Upgrader._GetCurrentVersions"""

  def _TestGetCurrentVersionsLocalCpv(self, target_infolist):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)
    self._SetUpPlayground()

    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(cpu.Upgrader, '_GetPreOrderDepGraph')

    # Replay script
    packages = [pinfo['package'] for pinfo in target_infolist]
    targets = ['=' + pinfo['cpv'] for pinfo in target_infolist]
    pm_argv = cpu.Upgrader._GenParallelEmergeArgv(mocked_upgrader, targets)
    pm_argv.append('--root-deps')
    verifier = _GenDepsGraphVerifier(packages)
    mocked_upgrader._GenParallelEmergeArgv(targets).AndReturn(pm_argv)
    mocked_upgrader._SetPortTree(mox.IsA(portcfg.config), mox.IsA(dict))
    cpu.Upgrader._GetPreOrderDepGraph(mox.Func(verifier)).AndReturn([])
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._GetCurrentVersions(mocked_upgrader, target_infolist)
    self.mox.VerifyAll()

    self._TearDownPlayground()

    return result

  def testGetCurrentVersionsTwoPkgs(self):
    target_infolist = [{'package': 'dev-libs/A', 'cpv': 'dev-libs/A-2'},
                       {'package': 'dev-libs/D', 'cpv': 'dev-libs/D-3'},
                       ]
    self._TestGetCurrentVersionsLocalCpv(target_infolist)

  def testGetCurrentVersionsOnePkgB(self):
    target_infolist = [{'package': 'dev-libs/B', 'cpv': 'dev-libs/B-2'},
                       ]
    self._TestGetCurrentVersionsLocalCpv(target_infolist)

  def testGetCurrentVersionsOnePkgLibcros(self):
    target_infolist = [{'package': 'chromeos-base/libcros',
                        'cpv': 'chromeos-base/libcros-1',
                        },
                       ]
    self._TestGetCurrentVersionsLocalCpv(target_infolist)

  def _TestGetCurrentVersionsPackageOnly(self, target_infolist):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)
    self._SetUpPlayground()

    # Add test-specific mocks/stubs
    self.mox.StubOutWithMock(cpu.Upgrader, '_GetPreOrderDepGraph')

    # Replay script
    packages = [pinfo['package'] for pinfo in target_infolist]
    pm_argv = cpu.Upgrader._GenParallelEmergeArgv(mocked_upgrader, packages)
    pm_argv.append('--root-deps')
    mocked_upgrader._GenParallelEmergeArgv(packages).AndReturn(pm_argv)
    mocked_upgrader._SetPortTree(mox.IsA(portcfg.config), mox.IsA(dict))
    cpu.Upgrader._GetPreOrderDepGraph(mox.IgnoreArg()).AndReturn([])
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._GetCurrentVersions(mocked_upgrader, target_infolist)
    self.mox.VerifyAll()

    self._TearDownPlayground()

    return result

  def testGetCurrentVersionsWorld(self):
    target_infolist = [{'package': 'world',
                        'cpv': 'world',
                        },
                       ]
    self._TestGetCurrentVersionsPackageOnly(target_infolist)

  def testGetCurrentVersionsLocalOnlyB(self):
    target_infolist = [{'package': 'dev-libs/B',
                        'cpv': None,
                        },
                       ]
    self._TestGetCurrentVersionsPackageOnly(target_infolist)

################################
### ResolveAndVerifyArgsTest ###
################################

class ResolveAndVerifyArgsTest(CpuTestBase):
  """Test Upgrader._ResolveAndVerifyArgs"""

  def _TestResolveAndVerifyArgsWorld(self, upgrade_mode):
    args = ['world']
    cmdargs=[]
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)

    # Add test-specific mocks/stubs

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._ResolveAndVerifyArgs(mocked_upgrader, args,
                                                upgrade_mode=upgrade_mode)
    self.mox.VerifyAll()

    self.assertEquals(result, [{'user_arg': 'world',
                                'package': 'world',
                                'package_name': 'world',
                                'category': None,
                                'cpv': 'world',
                                }])

  def testResolveAndVerifyArgsWorldUpgradeMode(self):
    self._TestResolveAndVerifyArgsWorld(True)

  def testResolveAndVerifyArgsWorldStatusMode(self):
    self._TestResolveAndVerifyArgsWorld(False)

  def _TestResolveAndVerifyArgsNonWorld(self, argdicts, cmdargs=[]):
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)
    upgrade_mode = cpu.Upgrader._IsInUpgradeMode(mocked_upgrader)

    # Add test-specific mocks/stubs

    # Replay script
    args = []
    for argdict in argdicts:
      arg = argdict['user_arg']
      local_cpv = argdict.get('cpv', None)
      upstream_cpv = argdict.get('upstream_cpv', None)
      args.append(arg)

      catpkg = cpu.Upgrader._GetCatPkgFromCpv(arg)
      verrev = cpu.Upgrader._GetVerRevFromCpv(arg)
      local_arg = catpkg if catpkg else arg

      mocked_upgrader._FindCurrentCPV(local_arg).AndReturn(local_cpv)
      mocked_upgrader._FindUpstreamCPV(arg, mocked_upgrader._unstable_ok,
                                       ).AndReturn(upstream_cpv)

      if not upstream_cpv and verrev:
        # Real method raises an exception here.
        if not mocked_upgrader._unstable_ok:
          mocked_upgrader._FindUpstreamCPV(arg, True).AndReturn(arg)
        break

      any_cpv = local_cpv if local_cpv else upstream_cpv
      if any_cpv:
        mocked_upgrader._FillInfoFromCPV(mox.IsA(dict), any_cpv)

    self.mox.ReplayAll()

    # Verify
    result = cpu.Upgrader._ResolveAndVerifyArgs(mocked_upgrader, args,
                                                upgrade_mode)
    self.mox.VerifyAll()

    return result

  def testResolveAndVerifyArgsNonWorldUpgrade(self):
    args = [{'user_arg': 'dev-libs/B',
             'cpv': 'dev-libs/B-1',
             'upstream_cpv': 'dev-libs/B-2',
             },
            ]
    cmdargs = ['--upgrade', '--unstable-ok']
    result = self._TestResolveAndVerifyArgsNonWorld(args, cmdargs)
    self.assertEquals(result, args)

  def testResolveAndVerifyArgsNonWorldUpgradeSpecificVer(self):
    args = [{'user_arg': 'dev-libs/B-2',
             'cpv': 'dev-libs/B-1',
             'upstream_cpv': 'dev-libs/B-2',
             },
            ]
    cmdargs = ['--upgrade', '--unstable-ok']
    result = self._TestResolveAndVerifyArgsNonWorld(args, cmdargs)
    self.assertEquals(result, args)

  def testResolveAndVerifyArgsNonWorldUpgradeSpecificVerNotFoundStable(self):
    args = [{'user_arg': 'dev-libs/B-2',
             'cpv': 'dev-libs/B-1',
             },
            ]
    cmdargs = ['--upgrade']
    try:
      result = self._TestResolveAndVerifyArgsNonWorld(args, cmdargs)
      self.assertTrue(False, "Should have raised RuntimeError")
    except RuntimeError as ex:
      # RuntimeError text should mention 'unstable'
      text = str(ex)
      msg = "No mention of 'is unstable' in error message: %s" % text
      self.assertTrue(0 <= text.find('is unstable'), msg)

  def testResolveAndVerifyArgsNonWorldUpgradeSpecificVerNotFoundUnstable(self):
    args = [{'user_arg': 'dev-libs/B-2',
             'cpv': 'dev-libs/B-1',
             },
            ]
    cmdargs = ['--upgrade', '--unstable-ok']
    try:
      result = self._TestResolveAndVerifyArgsNonWorld(args, cmdargs)
      self.assertTrue(False, "Should have raised RuntimeError")
    except RuntimeError as ex:
      # RuntimeError text should mention 'Unable to find'
      text = str(ex)
      msg = "Error message expected to start with 'Unable to find': %s" % text
      self.assertTrue(text.startswith('Unable to find'), msg)

  def testResolveAndVerifyArgsNonWorldLocalOnly(self):
    args = [{'user_arg': 'dev-libs/B',
             'cpv': 'dev-libs/B-1',
             },
            ]
    cmdargs = ['--upgrade', '--unstable-ok']
    result = self._TestResolveAndVerifyArgsNonWorld(args, cmdargs)
    self.assertEquals(result, args)

  def testResolveAndVerifyArgsNonWorldUpstreamOnly(self):
    args = [{'user_arg': 'dev-libs/B',
             'upstream_cpv': 'dev-libs/B-2',
             },
            ]
    cmdargs = ['--upgrade', '--unstable-ok']
    result = self._TestResolveAndVerifyArgsNonWorld(args, cmdargs)
    self.assertEquals(result, args)

  def testResolveAndVerifyArgsNonWorldNeither(self):
    args = [{'user_arg': 'dev-libs/B',
             },
            ]
    cmdargs = ['--upgrade', '--unstable-ok']
    self.assertRaises(RuntimeError,
                      self._TestResolveAndVerifyArgsNonWorld,
                      args, cmdargs)

  def testResolveAndVerifyArgsNonWorldStatusSpecificVer(self):
    """Exception because specific cpv arg not allowed without --ugprade."""
    cmdargs = ['--unstable-ok']
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)
    upgrade_mode = cpu.Upgrader._IsInUpgradeMode(mocked_upgrader)

    # Add test-specific mocks/stubs

    # Replay script
    self.mox.ReplayAll()

    # Verify
    self.assertRaises(RuntimeError,
                      cpu.Upgrader._ResolveAndVerifyArgs,
                      mocked_upgrader,
                      ['dev-libs/B-2'], upgrade_mode)
    self.mox.VerifyAll()


###############################
### GetPreOrderDepGraphTest ###
###############################

class GetPreOrderDepGraphTest(CpuTestBase):
  """Test the Upgrader class from cros_portage_upgrade."""

  #
  # _GetPreOrderDepGraph testing (defunct - to be replaced)
  #

  def _TestGetPreOrderDepGraph(self, pkg):
    """Test the behavior of the Upgrader._GetPreOrderDepGraph method."""

    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)
    self._SetUpPlayground()

    # Replay script
    self.mox.ReplayAll()

    # Verify
    pm_argv = cpu.Upgrader._GenParallelEmergeArgv(mocked_upgrader, [pkg])
    pm_argv.append('--root-deps')
    deps = parallel_emerge.DepGraphGenerator()
    deps.Initialize(pm_argv)
    deps_tree, deps_info = deps.GenDependencyTree()
    deps_graph = deps.GenDependencyGraph(deps_tree, deps_info)

    deps_list = cpu.Upgrader._GetPreOrderDepGraph(deps_graph)
    golden_deps_set = _GetGoldenDepsSet(pkg)
    self.assertEquals(set(deps_list), golden_deps_set)
    self.mox.VerifyAll()

    self._TearDownPlayground()

  def testGetPreOrderDepGraphDevLibsA(self):
    return self._TestGetPreOrderDepGraph('dev-libs/A')

  def testGetPreOrderDepGraphDevLibsC(self):
    return self._TestGetPreOrderDepGraph('dev-libs/C')

  def testGetPreOrderDepGraphVirtualLibusb(self):
    return self._TestGetPreOrderDepGraph('virtual/libusb')

  def testGetPreOrderDepGraphCrosbaseLibcros(self):
    return self._TestGetPreOrderDepGraph('chromeos-base/libcros')

################
### MainTest ###
################

class MainTest(CpuTestBase):
  """Test argument handling at the main method level."""

  def _PrepareArgv(self, *args):
    """Prepare command line for calling cros_portage_upgrade.main"""
    sys.argv = [ re.sub("_unittest", "", sys.argv[0]) ]
    sys.argv.extend(args)

  def _AssertOutputEndsInError(self, stdout, debug=False):
    """Return True if |stdout| ends with an error message."""
    if stdout:
      lastline = [ln for ln in stdout.split('\n') if ln][-1]
      if debug:
        print("Test is expecting error in last line of following output:\n%s" %
              stdout)
      self.assertTrue(_IsErrorLine(lastline),
                      msg="expected output to end in error line, but "
                      "_IsErrorLine says this line is not an error:\n%s" %
                      lastline)
    else:
      self.assertTrue(stdout,
                      msg="expected output with error, but no output found.")

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

  def testUpgradeAndUpgradeDeep(self):
    """Running with --upgrade and --upgrade-deep exits with an error."""
    self._PrepareArgv("--host", "--upgrade", "--upgrade-deep", "any-package")

    # Capture stdout/stderr so it can be verified later
    self._StartCapturingOutput()

    # Expect exit with code!=0
    self._AssertCPUMain(cpu, expect_zero=False)

    # Verify that an error message was printed.
    (stdout, stderr) = self._RetrieveCapturedOutput()
    self._StopCapturingOutput()
    self._AssertOutputEndsInError(stdout)

  def testForceWithoutUpgrade(self):
    """Running with --force requires --upgrade or --upgrade-deep."""
    self._PrepareArgv("--host", "--force", "any-package")

    # Capture stdout/stderr so it can be verified later
    self._StartCapturingOutput()

    # Expect exit with code!=0
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
