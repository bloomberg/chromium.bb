# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Collection of tests to run on the rootfs of a built image.

This module should only be imported inside the chroot.
"""

from __future__ import print_function

import cStringIO
import collections
import itertools
import lddtree
import magic
import mimetypes
import os
import re
import stat

from elftools.elf import elffile
from elftools.common import exceptions

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import filetype
from chromite.lib import image_test_lib
from chromite.lib import osutils
from chromite.lib import parseelf


class LocaltimeTest(image_test_lib.NonForgivingImageTestCase):
  """Verify that /etc/localtime is a symlink to /var/lib/timezone/localtime.

  This is an example of an image test. The image is already mounted. The
  test can access rootfs via ROOT_A constant.
  """

  def TestLocaltimeIsSymlink(self):
    localtime_path = os.path.join(image_test_lib.ROOT_A, 'etc', 'localtime')
    self.assertTrue(os.path.islink(localtime_path))

  def TestLocaltimeLinkIsCorrect(self):
    localtime_path = os.path.join(image_test_lib.ROOT_A, 'etc', 'localtime')
    self.assertEqual('/var/lib/timezone/localtime',
                     os.readlink(localtime_path))


def _GuessMimeType(magic_obj, file_name):
  """Guess a file's mimetype base on its extension and content.

  File extension is favored over file content to reduce noise.

  Args:
    magic_obj: A loaded magic instance.
    file_name: A path to the file.

  Returns:
    A mime type of |file_name|.
  """
  mime_type, _ = mimetypes.guess_type(file_name)
  if not mime_type:
    mime_type = magic_obj.file(file_name)
  return mime_type


class BlacklistTest(image_test_lib.NonForgivingImageTestCase):
  """Verify that rootfs does not contain blacklisted items."""

  def TestBlacklistedDirectories(self):
    dirs = [os.path.join(image_test_lib.ROOT_A, 'usr', 'share', 'locale')]
    for d in dirs:
      self.assertFalse(os.path.isdir(d), 'Directory %s is blacklisted.' % d)

  def TestBlacklistedFileTypes(self):
    """Fail if there are files of prohibited types (such as C++ source code).

    The whitelist has higher precedence than the blacklist.
    """
    blacklisted_patterns = [re.compile(x) for x in [
        r'text/x-c\+\+',
        r'text/x-c',
    ]]
    whitelisted_patterns = [re.compile(x) for x in [
        r'.*/braille/.*',
        r'.*/brltty/.*',
        r'.*/etc/sudoers$',
        r'.*/dump_vpd_log$',
        r'.*\.conf$',
        r'.*/libnl/classid$',
        r'.*/locale/',
        r'.*/X11/xkb/',
        r'.*/chromeos-assets/',
        r'.*/udev/rules.d/',
        r'.*/firmware/ar3k/.*pst$',
        r'.*/etc/services',
        r'.*/usr/share/dev-install/portage',
    ]]

    failures = []

    magic_obj = magic.open(magic.MAGIC_MIME_TYPE)
    magic_obj.load()
    for root, _, file_names in os.walk(image_test_lib.ROOT_A):
      for file_name in file_names:
        full_name = os.path.join(root, file_name)
        if os.path.islink(full_name) or not os.path.isfile(full_name):
          continue

        mime_type = _GuessMimeType(magic_obj, full_name)
        if (any(x.match(mime_type) for x in blacklisted_patterns) and not
            any(x.match(full_name) for x in whitelisted_patterns)):
          failures.append('File %s has blacklisted type %s.' %
                          (full_name, mime_type))
    magic_obj.close()

    self.assertFalse(failures, '\n'.join(failures))

  def TestValidInterpreter(self):
    """Fail if a script's interpreter is not found, or not executable.

    A script interpreter is anything after the #! sign, up to the end of line
    or the first space.
    """
    failures = []

    for root, _, file_names in os.walk(image_test_lib.ROOT_A):
      for file_name in file_names:
        full_name = os.path.join(root, file_name)
        file_stat = os.lstat(full_name)
        if (not stat.S_ISREG(file_stat.st_mode) or
            (file_stat.st_mode & 0111) == 0):
          continue

        with open(full_name, 'rb') as f:
          if f.read(2) != '#!':
            continue
          line = '#!' + f.readline().strip()

        try:
          # Ignore arguments to the interpreter.
          interp, _ = filetype.SplitShebang(line)
        except ValueError:
          failures.append('File %s has an invalid interpreter path: "%s".' %
                          (full_name, line))

        # Absolute path to the interpreter.
        interp = os.path.join(image_test_lib.ROOT_A, interp.lstrip('/'))
        # Interpreter could be a symlink. Resolve it.
        interp = osutils.ResolveSymlink(interp, image_test_lib.ROOT_A)
        if not os.path.isfile(interp):
          failures.append('File %s uses non-existing interpreter %s.' %
                          (full_name, interp))
        elif (os.stat(interp).st_mode & 0111) == 0:
          failures.append('Interpreter %s is not executable.' % interp)

    self.assertFalse(failures, '\n'.join(failures))


class LinkageTest(image_test_lib.NonForgivingImageTestCase):
  """Verify that all binaries and libraries have proper linkage."""

  def setUp(self):
    osutils.MountDir(
        os.path.join(image_test_lib.STATEFUL, 'var_overlay'),
        os.path.join(image_test_lib.ROOT_A, 'var'),
        mount_opts=('bind', ),
    )

  def tearDown(self):
    osutils.UmountDir(
        os.path.join(image_test_lib.ROOT_A, 'var'),
        cleanup=False,
    )

  def _IsPackageMerged(self, package_name):
    cmd = [
        'portageq',
        'has_version',
        image_test_lib.ROOT_A,
        package_name
    ]
    ret = cros_build_lib.RunCommand(cmd, error_code_ok=True,
                                    combine_stdout_stderr=True,
                                    extra_env={'ROOT': image_test_lib.ROOT_A})
    if ret.returncode == 0:
      logging.info('Package is available: %s', package_name)
    else:
      logging.info('Package is not available: %s', package_name)
    return ret.returncode == 0

  def TestLinkage(self):
    """Find main executable binaries and check their linkage."""
    binaries = [
        'boot/vmlinuz',
        'bin/sed',
    ]

    if self._IsPackageMerged('chromeos-base/chromeos-login'):
      binaries.append('sbin/session_manager')

    if self._IsPackageMerged('x11-base/xorg-server'):
      binaries.append('usr/bin/Xorg')

    # When chrome is built with USE="pgo_generate", rootfs chrome is actually a
    # symlink to a real binary which is in the stateful partition. So we do not
    # check for a valid chrome binary in that case.
    if not self._IsPackageMerged('chromeos-base/chromeos-chrome[pgo_generate]'):
      if self._IsPackageMerged('chromeos-base/chromeos-chrome[app_shell]'):
        binaries.append('opt/google/chrome/app_shell')
      elif self._IsPackageMerged('chromeos-base/chromeos-chrome[envoy]'):
        binaries.append('opt/google/chrome/envoy_shell')
      elif self._IsPackageMerged('chromeos-base/chromeos-chrome'):
        binaries.append('opt/google/chrome/chrome')

    binaries = [os.path.join(image_test_lib.ROOT_A, x) for x in binaries]

    # Grab all .so files
    libraries = []
    for root, _, files in os.walk(image_test_lib.ROOT_A):
      for name in files:
        filename = os.path.join(root, name)
        if '.so' in filename:
          libraries.append(filename)

    ldpaths = lddtree.LoadLdpaths(image_test_lib.ROOT_A)
    for to_test in itertools.chain(binaries, libraries):
      # to_test could be a symlink, we need to resolve it relative to ROOT_A.
      while os.path.islink(to_test):
        link = os.readlink(to_test)
        if link.startswith('/'):
          to_test = os.path.join(image_test_lib.ROOT_A, link[1:])
        else:
          to_test = os.path.join(os.path.dirname(to_test), link)
      try:
        lddtree.ParseELF(to_test, root=image_test_lib.ROOT_A, ldpaths=ldpaths)
      except lddtree.exceptions.ELFError:
        continue
      except IOError as e:
        self.fail('Fail linkage test for %s: %s' % (to_test, e))


class FileSystemMetaDataTest(image_test_lib.ForgivingImageTestCase):
  """A test class to gather file system stats such as free inodes, blocks."""

  def TestStats(self):
    """Collect inodes and blocks usage."""
    # Find the loopback device that was mounted to ROOT_A.
    loop_device = None
    root_path = os.path.abspath(os.readlink(image_test_lib.ROOT_A))
    for mtab in osutils.IterateMountPoints():
      if mtab.destination == root_path:
        loop_device = mtab.source
        break
    self.assertTrue(loop_device, 'Cannot find loopback device for ROOT_A.')

    # Gather file system stats with tune2fs.
    cmd = [
        'tune2fs',
        '-l',
        loop_device
    ]
    # tune2fs produces output like this:
    #
    # tune2fs 1.42 (29-Nov-2011)
    # Filesystem volume name:   ROOT-A
    # Last mounted on:          <not available>
    # Filesystem UUID:          <none>
    # Filesystem magic number:  0xEF53
    # Filesystem revision #:    1 (dynamic)
    # ...
    #
    # So we need to ignore the first line.
    ret = cros_build_lib.SudoRunCommand(cmd, capture_output=True,
                                        extra_env={'LC_ALL': 'C'})
    fs_stat = dict(line.split(':', 1) for line in ret.output.splitlines()
                   if ':' in line)
    free_inodes = int(fs_stat['Free inodes'])
    free_blocks = int(fs_stat['Free blocks'])
    inode_count = int(fs_stat['Inode count'])
    block_count = int(fs_stat['Block count'])
    block_size = int(fs_stat['Block size'])

    sum_file_size = 0
    for root, _, filenames in os.walk(image_test_lib.ROOT_A):
      for file_name in filenames:
        full_name = os.path.join(root, file_name)
        file_stat = os.lstat(full_name)
        sum_file_size += file_stat.st_size

    metadata_size = (block_count - free_blocks) * block_size - sum_file_size

    self.OutputPerfValue('free_inodes_over_inode_count',
                         free_inodes * 100.0 / inode_count, 'percent',
                         graph='free_over_used_ratio')
    self.OutputPerfValue('free_blocks_over_block_count',
                         free_blocks * 100.0 / block_count, 'percent',
                         graph='free_over_used_ratio')
    self.OutputPerfValue('apparent_size', sum_file_size, 'bytes',
                         higher_is_better=False, graph='filesystem_stats')
    self.OutputPerfValue('metadata_size', metadata_size, 'bytes',
                         higher_is_better=False, graph='filesystem_stats')


class SymbolsTest(image_test_lib.NonForgivingImageTestCase):
  """Tests related to symbols in ELF files."""

  def setUp(self):
    # Mapping of file name --> 2-tuple (import, export).
    self._known_symtabs = {}

  def _GetSymbols(self, file_name):
    """Return a 2-tuple (import, export) of an ELF file |file_name|.

    Import and export in the returned tuple are sets of strings (symbol names).
    """
    if file_name in self._known_symtabs:
      return self._known_symtabs[file_name]

    # We use cstringio here to obviate fseek/fread time in pyelftools.
    stream = cStringIO.StringIO(osutils.ReadFile(file_name))

    try:
      elf = elffile.ELFFile(stream)
    except exceptions.ELFError:
      raise ValueError('%s is not an ELF file.' % file_name)

    imp, exp = parseelf.ParseELFSymbols(elf)
    exp = set(exp.keys())

    self._known_symtabs[file_name] = imp, exp
    return imp, exp

  def TestImportedSymbolsAreAvailable(self):
    """Ensure all ELF files' imported symbols are available in ROOT-A.

    In this test, we find all imported symbols and exported symbols from all
    ELF files on the system. This test will fail if the set of imported symbols
    is not a subset of exported symbols.

    This test DOES NOT simulate ELF loading. "TestLinkage" does that with
    `lddtree`.
    """
    # Import tables of files, keyed by file names.
    importeds = collections.defaultdict(set)
    # All exported symbols.
    exported = set()

    for root, _, filenames in os.walk(image_test_lib.ROOT_A):
      for filename in filenames:
        full_name = os.path.join(root, filename)
        if os.path.islink(full_name) or not os.path.isfile(full_name):
          continue

        try:
          imp, exp = self._GetSymbols(full_name)
        except (ValueError, IOError):
          continue
        else:
          importeds[full_name] = imp
          exported.update(exp)

    known_unsatisfieds = {
        'libthread_db-1.0.so': set([
            'ps_pdwrite', 'ps_pdread', 'ps_lgetfpregs', 'ps_lsetregs',
            'ps_lgetregs', 'ps_lsetfpregs', 'ps_pglobal_lookup', 'ps_getpid']),
    }

    failures = []
    for full_name, imported in importeds.iteritems():
      file_name = os.path.basename(full_name)
      missing = imported - exported - known_unsatisfieds.get(file_name, set())
      if missing:
        failures.append('File %s contains unsatisfied symbols: %r' %
                        (full_name, missing))
    self.assertFalse(failures, '\n'.join(failures))
