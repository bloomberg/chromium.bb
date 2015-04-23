# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for workon_helper."""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import portage_util
from chromite.lib import sysroot_lib
from chromite.lib import osutils
from chromite.lib import workon_helper


BOARD = 'this_is_a_board_name'

WORKON_ONLY_ATOM = 'sys-apps/my-package'
VERSIONED_WORKON_ATOM = 'sys-apps/versioned-package'
NOT_WORKON_ATOM = 'sys-apps/not-workon-package'

HOST_ATOM = 'host-apps/my-package'

WORKED_ON_PATTERN = '=%s-9999'
MASKED_PATTERN = '<%s-9999'

OVERLAY_ROOT_DIR = 'overlays'
BOARD_OVERLAY_DIR = 'overlay-' + BOARD
HOST_OVERLAY_DIR = 'overlay-host'


class WorkonHelperTest(cros_test_lib.MockTempDirTestCase):
  """Tests for chromite.lib.workon_helper."""

  def _MakeFakeEbuild(self, overlay, atom, version, is_workon=True):
    """Makes fake ebuilds with minimal real content.

    Args:
      overlay: overlay to put this ebuild in.
      atom: 'category/package' string in the familiar portage sense.
      version: version suffix for the ebuild (e.g. '9999').
      is_workon: True iff this should be a workon-able package
          (i.e. inherits cros-workon).
    """
    category, package = atom.split('/', 1)
    ebuild_path = os.path.join(self._mock_srcdir, OVERLAY_ROOT_DIR, overlay,
                               category, package,
                               '%s-%s.ebuild' % (package, version))
    content = 'KEYWORDS="~*"\n'
    if is_workon:
      content += 'inherit cros-workon\n'
    osutils.WriteFile(ebuild_path, content, makedirs=True)
    if atom not in self._valid_atoms:
      self._valid_atoms[atom] = ebuild_path

  def _MockFindOverlays(self, sysroot):
    """Mocked out version of portage_util.FindOverlays().

    Args:
      sysroot: path to sysroot.

    Returns:
      List of paths to overlays.
    """
    if sysroot == '/':
      return [os.path.join(self._overlay_root, HOST_OVERLAY_DIR)]
    return [os.path.join(self._overlay_root, BOARD_OVERLAY_DIR)]

  def _MockFindEbuildForPackage(self, package, _board=None, **_kwargs):
    """Mocked out version of portage_util.FindEbuildForPackage().

    Args:
      package: complete atom string.
      _board: ignored, see documentation in portage_util.  We intentionally
          create atoms with different names for hosts/boards so that we can
          ignore this distinction here.
      _kwargs: ignored, see documentation in portage_util.

    Returns:
      An ebuild if we have previously created this atom.
    """
    return self._valid_atoms.get(package, None)

  def setUp(self):
    """Set up a test environment."""
    self._valid_atoms = dict()
    self._mock_srcdir = os.path.join(self.tempdir, 'src')
    workon_dir = workon_helper.GetWorkonPath(source_root=self._mock_srcdir)
    self._board_sysroot = os.path.join(self.tempdir, 'board_sysroot')
    self._host_sysroot = os.path.join(self.tempdir, 'host_sysroot')
    osutils.SafeMakedirs(self._board_sysroot)
    osutils.SafeMakedirs(self._host_sysroot)
    osutils.SafeMakedirs(self._mock_srcdir)
    for system in ('host', BOARD):
      osutils.Touch(os.path.join(workon_dir, system), makedirs=True)
      osutils.Touch(os.path.join(workon_dir, system + '.mask'), makedirs=True)
    self._overlay_root = os.path.join(self._mock_srcdir, OVERLAY_ROOT_DIR)
    # Make a bunch of packages to work on.
    self._MakeFakeEbuild(BOARD_OVERLAY_DIR, WORKON_ONLY_ATOM, '9999')
    self._MakeFakeEbuild(BOARD_OVERLAY_DIR, VERSIONED_WORKON_ATOM, '9999')
    self._MakeFakeEbuild(BOARD_OVERLAY_DIR, VERSIONED_WORKON_ATOM, '0.0.1-r1')
    self._MakeFakeEbuild(BOARD_OVERLAY_DIR, NOT_WORKON_ATOM, '0.0.1-r1',
                         is_workon=False)
    self._MakeFakeEbuild(HOST_OVERLAY_DIR, HOST_ATOM, '9999')
    # Patch the modules interfaces to the rest of the world.
    self.PatchObject(portage_util, 'FindEbuildForPackage',
                     self._MockFindEbuildForPackage)
    # This basically turns off behavior related to adding repositories to
    # minilayouts.
    self.PatchObject(git.ManifestCheckout, 'IsFullManifest', return_value=True)
    self.PatchObject(
        portage_util, 'GetRepositoryForEbuild', return_value=(
            portage_util.RepositoryInfoTuple(srcdir=self._mock_srcdir,
                                             project='workon-project'),
        )
    )
    host_overlay = os.path.join(self._overlay_root, HOST_OVERLAY_DIR)
    board_overlay = os.path.join(self._overlay_root, BOARD_OVERLAY_DIR)

    # Setup the sysroots.
    sysroot_lib.Sysroot(self._board_sysroot).WriteConfig(
        'ARCH="amd64"\nPORTDIR_OVERLAY="%s"' % board_overlay)
    sysroot_lib.Sysroot(self._host_sysroot).WriteConfig(
        'ARCH="amd64"\nPORTDIR_OVERLAY="%s"' % host_overlay)

    # Create helpers for both host and board.
    self.helper = workon_helper.WorkonHelper(
        self._board_sysroot, BOARD, src_root=self._mock_srcdir)
    self.host_helper = workon_helper.WorkonHelper(
        self._host_sysroot, 'host', src_root=self._mock_srcdir)

  def tearDown(self):
    """Clean up the test environment."""
    # sysroots are created with root permissions, remove them with root
    # permission too.
    osutils.RmDir(self._host_sysroot, sudo=True)
    osutils.RmDir(self._board_sysroot, sudo=True)

  def assertWorkingOn(self, atoms, system=BOARD):
    """Assert that the workon/mask files mention the given atoms.

    Args:
      atoms: list of atom strings (e.g. ['sys-apps/dbus', 'foo-cat/bar']).
      system: string system to consider (either 'host' or a board name).
    """
    workon_path = workon_helper.GetWorkonPath(
        source_root=self._mock_srcdir, sub_path=system)
    self.assertEqual(sorted([WORKED_ON_PATTERN % atom for atom in atoms]),
                     sorted(osutils.ReadFile(workon_path).splitlines()))
    mask_path = workon_path + '.mask'
    self.assertEqual(sorted([MASKED_PATTERN % atom for atom in atoms]),
                     sorted(osutils.ReadFile(mask_path).splitlines()))

  def testShouldDetectBoardNotSetUp(self):
    """Check that we complain if a board has not been previously setup."""
    with self.assertRaises(workon_helper.WorkonError):
      workon_helper.WorkonHelper(os.path.join(self.tempdir, 'nonexistent'),
                                 'this-board-is-not-setup.',
                                 src_root=self._mock_srcdir)

  def testCanStartSingleAtom(self):
    """Check that we can mark a single atom as being worked on."""
    self.helper.StartWorkingOnPackages([WORKON_ONLY_ATOM])
    self.assertWorkingOn([WORKON_ONLY_ATOM])

  def testCanStartMultipleAtoms(self):
    """Check that we can mark a multiple atoms as being worked on."""
    expected_atoms = (WORKON_ONLY_ATOM, VERSIONED_WORKON_ATOM)
    self.helper.StartWorkingOnPackages(expected_atoms)
    self.assertWorkingOn(expected_atoms)

  def testCanStartAtomsWithAll(self):
    """Check that we can mark all possible workon atoms as started."""
    expected_atoms = (WORKON_ONLY_ATOM, VERSIONED_WORKON_ATOM)
    self.helper.StartWorkingOnPackages([], use_all=True)
    self.assertWorkingOn(expected_atoms)

  def testCanStartAtomsWithWorkonOnly(self):
    """Check that we can start atoms that have only a cros-workon ebuild."""
    expected_atoms = (WORKON_ONLY_ATOM,)
    self.helper.StartWorkingOnPackages([], use_workon_only=True)
    self.assertWorkingOn(expected_atoms)

  def testCannotStartAtomTwice(self):
    """Check that starting an atom twice has no effect."""
    self.helper.StartWorkingOnPackages([WORKON_ONLY_ATOM])
    self.helper.StartWorkingOnPackages([WORKON_ONLY_ATOM])
    self.assertWorkingOn([WORKON_ONLY_ATOM])

  def testCanStopSingleAtom(self):
    """Check that we can stop a previously started atom."""
    self.helper.StartWorkingOnPackages([WORKON_ONLY_ATOM])
    self.assertWorkingOn([WORKON_ONLY_ATOM])
    self.helper.StopWorkingOnPackages([WORKON_ONLY_ATOM])
    self.assertWorkingOn([])

  def testCanStopMultipleAtoms(self):
    """Check that we can stop multiple previously worked on atoms."""
    expected_atoms = (WORKON_ONLY_ATOM, VERSIONED_WORKON_ATOM)
    self.helper.StartWorkingOnPackages(expected_atoms)
    self.assertWorkingOn(expected_atoms)
    self.helper.StopWorkingOnPackages([WORKON_ONLY_ATOM])
    self.assertWorkingOn([VERSIONED_WORKON_ATOM])
    self.helper.StopWorkingOnPackages([VERSIONED_WORKON_ATOM])
    self.assertWorkingOn([])
    # Now do it all at once.
    self.helper.StartWorkingOnPackages(expected_atoms)
    self.assertWorkingOn(expected_atoms)
    self.helper.StopWorkingOnPackages(expected_atoms)
    self.assertWorkingOn([])

  def testCanStopAtomsWithAll(self):
    """Check that we can stop all worked on atoms."""
    expected_atoms = (WORKON_ONLY_ATOM, VERSIONED_WORKON_ATOM)
    self.helper.StartWorkingOnPackages(expected_atoms)
    self.helper.StopWorkingOnPackages([], use_all=True)
    self.assertWorkingOn([])

  def testCanStopAtomsWithWorkonOnly(self):
    """Check that we can stop all workon only atoms."""
    expected_atoms = (WORKON_ONLY_ATOM, VERSIONED_WORKON_ATOM)
    self.helper.StartWorkingOnPackages(expected_atoms)
    self.helper.StopWorkingOnPackages([], use_workon_only=True)
    self.assertWorkingOn([VERSIONED_WORKON_ATOM])

  def testShouldDetectUnknownAtom(self):
    """Check that we reject requests to work on unknown atoms."""
    with self.assertRaises(workon_helper.WorkonError):
      self.helper.StopWorkingOnPackages(['sys-apps/not-a-thing'])

  def testCanListAllWorkedOnAtoms(self):
    """Check that we can list all worked on atoms across boards."""
    self.assertEqual(dict(),
                     workon_helper.ListAllWorkedOnAtoms(
                         src_root=self._mock_srcdir))
    self.helper.StartWorkingOnPackages([WORKON_ONLY_ATOM])
    self.assertEqual({BOARD: [WORKON_ONLY_ATOM]},
                     workon_helper.ListAllWorkedOnAtoms(
                         src_root=self._mock_srcdir))
    self.host_helper.StartWorkingOnPackages([HOST_ATOM])
    self.assertEqual({BOARD: [WORKON_ONLY_ATOM], 'host': [HOST_ATOM]},
                     workon_helper.ListAllWorkedOnAtoms(
                         src_root=self._mock_srcdir))

  def testCanListWorkedOnAtoms(self):
    """Check that we can list the atoms we're currently working on."""
    self.assertEqual(self.helper.ListAtoms(), [])
    self.helper.StartWorkingOnPackages([WORKON_ONLY_ATOM])
    self.assertEqual(self.helper.ListAtoms(), [WORKON_ONLY_ATOM])

  def testCanListAtomsWithAll(self):
    """Check that we can list all possible atoms to work on."""
    self.assertEqual(sorted(self.helper.ListAtoms(use_all=True)),
                     sorted([WORKON_ONLY_ATOM, VERSIONED_WORKON_ATOM]))

  def testCanListAtomsWithWorkonOnly(self):
    """Check that we can list all workon only atoms."""
    self.assertEqual(self.helper.ListAtoms(use_workon_only=True),
                     [WORKON_ONLY_ATOM])

  def testCanRunCommand(self):
    """Test that we can run a command in package source directories."""
    file_name = 'foo'
    file_path = os.path.join(self._mock_srcdir, file_name)
    self.assertNotExists(file_path)
    self.helper.RunCommandInPackages([WORKON_ONLY_ATOM], 'touch %s' % file_name)
    self.assertExists(file_path)
