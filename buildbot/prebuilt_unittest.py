#!/usr/bin/python
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import mox
import os
import multiprocessing
import sys
import tempfile
import unittest

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
import prebuilt
from chromite.lib import cros_build_lib
from chromite.lib import binpkg

# pylint: disable=E1120,W0212,R0904
PUBLIC_PACKAGES = [{'CPV': 'gtk+/public1', 'SHA1': '1', 'MTIME': '1'},
                   {'CPV': 'gtk+/public2', 'SHA1': '2',
                    'PATH': 'gtk+/foo.tgz', 'MTIME': '2'}]
PRIVATE_PACKAGES = [{'CPV': 'private', 'SHA1': '3', 'MTIME': '3'}]


def SimplePackageIndex(header=True, packages=True):
  pkgindex = binpkg.PackageIndex()
  if header:
    pkgindex.header['URI'] = 'http://www.example.com'
  if packages:
    pkgindex.packages = copy.deepcopy(PUBLIC_PACKAGES + PRIVATE_PACKAGES)
  return pkgindex


class TestUpdateFile(unittest.TestCase):

  def setUp(self):
    self.contents_str = ['# comment that should be skipped',
                         'PKGDIR="/var/lib/portage/pkgs"',
                         'PORTAGE_BINHOST="http://no.thanks.com"',
                         'portage portage-20100310.tar.bz2',
                         'COMPILE_FLAGS="some_value=some_other"',
                         ]
    temp_fd, self.version_file = tempfile.mkstemp()
    os.write(temp_fd, '\n'.join(self.contents_str))
    os.close(temp_fd)

  def tearDown(self):
    os.remove(self.version_file)

  def _read_version_file(self, version_file=None):
    """Read the contents of self.version_file and return as a list."""
    if not version_file:
      version_file = self.version_file

    version_fh = open(version_file)
    try:
      return [line.strip() for line in version_fh.readlines()]
    finally:
      version_fh.close()

  def _verify_key_pair(self, key, val):
    file_contents = self._read_version_file()
    # ensure key for verify is wrapped on quotes
    if '"' not in val:
      val = '"%s"' % val
    for entry in file_contents:
      if '=' not in entry:
        continue
      file_key, file_val = entry.split('=')
      if file_key == key:
        if val == file_val:
          break
    else:
      self.fail('Could not find "%s=%s" in version file' % (key, val))

  def testAddVariableThatDoesNotExist(self):
    """Add in a new variable that was no present in the file."""
    key = 'PORTAGE_BINHOST'
    value = '1234567'
    prebuilt.UpdateLocalFile(self.version_file, value)
    print self.version_file
    self._read_version_file()
    self._verify_key_pair(key, value)
    print self.version_file

  def testUpdateVariable(self):
    """Test updating a variable that already exists."""
    key, val = self.contents_str[2].split('=')
    new_val = 'test_update'
    self._verify_key_pair(key, val)
    prebuilt.UpdateLocalFile(self.version_file, new_val)
    self._verify_key_pair(key, new_val)

  def testUpdateNonExistentFile(self):
    key = 'PORTAGE_BINHOST'
    value = '1234567'
    non_existent_file = tempfile.mktemp()
    try:
      prebuilt.UpdateLocalFile(non_existent_file, value)
      file_contents = self._read_version_file(non_existent_file)
      self.assertEqual(file_contents, ['%s=%s' % (key, value)])
    finally:
      if os.path.exists(non_existent_file):
        os.remove(non_existent_file)


class TestPrebuilt(unittest.TestCase):

  def setUp(self):
    self.mox = mox.Mox()

  def tearDown(self):
    self.mox.UnsetStubs()
    self.mox.VerifyAll()

  def testGenerateUploadDict(self):
    base_local_path = '/b/cbuild/build/chroot/build/x86-dogfood/'
    gs_bucket_path = 'gs://chromeos-prebuilt/host/version'
    local_path = os.path.join(base_local_path, 'public1.tbz2')
    self.mox.StubOutWithMock(prebuilt.os.path, 'exists')
    prebuilt.os.path.exists(local_path).AndReturn(True)
    self.mox.ReplayAll()
    pkgs = [{ 'CPV': 'public1' }]
    result = prebuilt.GenerateUploadDict(base_local_path, gs_bucket_path, pkgs)
    expected = { local_path: gs_bucket_path + '/public1.tbz2' }
    self.assertEqual(result, expected)

  def testFailonUploadFail(self):
    """Make sure we fail if one of the upload processes fail."""
    self.mox.StubOutWithMock(prebuilt, '_RetryRun')
    prebuilt._RetryRun(mox.IgnoreArg(), print_cmd=False).AndReturn(False)
    self.mox.ReplayAll()
    files = {'test': '/uasd'}
    result = prebuilt.RemoteUpload('public-read', files)
    self.assertEqual(result, set([('test', '/uasd')]))
    prebuilt._RetryRun('example', print_cmd=False)

  def testDeterminePrebuiltConfHost(self):
    """Test that the host prebuilt path comes back properly."""
    expected_path = os.path.join(prebuilt._PREBUILT_MAKE_CONF['amd64'])
    self.assertEqual(prebuilt.DeterminePrebuiltConfFile('fake_path', 'amd64'),
                     expected_path)

  def testDeterminePrebuiltConf(self):
    """Test the different known variants of boards for proper path discovery."""
    fake_path = '/b/cbuild'
    script_path = os.path.join(fake_path, 'src/platform/dev/host')
    public_overlay_path = os.path.join(fake_path, 'src/overlays')
    private_overlay_path = os.path.join(fake_path,
                                        prebuilt._PRIVATE_OVERLAY_DIR)
    path_dict = {'private_overlay_path': private_overlay_path,
                 'public_overlay_path': public_overlay_path}
    # format for targets
    # board target key in dictionar
    # Tuple containing cmd run, expected results as cmd obj, and expected output

    # Mock output from cros_overlay_list
    x86_out = ('%(private_overlay_path)s/chromeos-overlay\n'
               '%(public_overlay_path)s/overlay-x86-generic\n' % path_dict)

    x86_cmd = './cros_overlay_list --board x86-generic'
    x86_expected_path = os.path.join(public_overlay_path, 'overlay-x86-generic',
                                     'prebuilt.conf')
    # Mock output from cros_overlay_list
    tegra2_out = ('%(private_overlay_path)s/chromeos-overlay\n'
                  '%(public_overlay_path)s/overlay-tegra2\n'
                  '%(public_overlay_path)s/overlay-variant-tegra2-seaboard\n'
                  '%(private_overlay_path)s/overlay-tegra2-private\n'
                  '%(private_overlay_path)s/'
                  'overlay-variant-tegra2-seaboard-private\n' % path_dict)
    tegra2_cmd = './cros_overlay_list --board tegra2 --variant seaboard'
    tegra2_expected_path = os.path.join(
        private_overlay_path, 'overlay-variant-tegra2-seaboard-private',
        'prebuilt.conf')


    targets = {'x86-generic': {'cmd': x86_cmd,
                               'output': x86_out,
                               'result': x86_expected_path},
              'tegra2_seaboard': {'cmd': tegra2_cmd,
                                  'output': tegra2_out,
                                  'result': tegra2_expected_path}
              }

    self.mox.StubOutWithMock(prebuilt.cros_build_lib, 'RunCommand')
    for target, expected_results in targets.iteritems():
      # create command object for output
      cmd_result_obj = cros_build_lib.CommandResult()
      cmd_result_obj.output = expected_results['output']
      prebuilt.cros_build_lib.RunCommand(
        expected_results['cmd'].split(), redirect_stdout=True,
        cwd=script_path).AndReturn(cmd_result_obj)

    self.mox.ReplayAll()
    for target, expected_results in targets.iteritems():
      target = prebuilt.BuildTarget(target)
      self.assertEqual(
        prebuilt.DeterminePrebuiltConfFile(fake_path, target),
        expected_results['result'])


class TestPackagesFileFiltering(unittest.TestCase):

  def testFilterPkgIndex(self):
    pkgindex = SimplePackageIndex()
    pkgindex.RemoveFilteredPackages(lambda pkg: pkg in PRIVATE_PACKAGES)
    self.assertEqual(pkgindex.packages, PUBLIC_PACKAGES)
    self.assertEqual(pkgindex.modified, True)


class TestPopulateDuplicateDB(unittest.TestCase):

  def testEmptyIndex(self):
    pkgindex = SimplePackageIndex(packages=False)
    db = {}
    pkgindex._PopulateDuplicateDB(db, 0)
    self.assertEqual(db, {})

  def testNormalIndex(self):
    pkgindex = SimplePackageIndex()
    db = {}
    pkgindex._PopulateDuplicateDB(db, 0)
    self.assertEqual(len(db), 3)
    self.assertEqual(db['1'], 'http://www.example.com/gtk+/public1.tbz2')
    self.assertEqual(db['2'], 'http://www.example.com/gtk+/foo.tgz')
    self.assertEqual(db['3'], 'http://www.example.com/private.tbz2')

  def testMissingSHA1(self):
    db = {}
    pkgindex = SimplePackageIndex()
    del pkgindex.packages[0]['SHA1']
    pkgindex._PopulateDuplicateDB(db, 0)
    self.assertEqual(len(db), 2)
    self.assertEqual(db['2'], 'http://www.example.com/gtk+/foo.tgz')
    self.assertEqual(db['3'], 'http://www.example.com/private.tbz2')

  def testFailedPopulate(self):
    db = {}
    pkgindex = SimplePackageIndex(header=False)
    self.assertRaises(KeyError, pkgindex._PopulateDuplicateDB, db, 0)
    pkgindex = SimplePackageIndex()
    del pkgindex.packages[0]['CPV']
    self.assertRaises(KeyError, pkgindex._PopulateDuplicateDB, db, 0)


class TestResolveDuplicateUploads(unittest.TestCase):

  def setUp(self):
    self.mox = mox.Mox()
    self.mox.StubOutWithMock(binpkg.time, 'time')
    binpkg.time.time().AndReturn(binpkg.TWO_WEEKS)
    self.mox.ReplayAll()

  def tearDown(self):
    self.mox.UnsetStubs()
    self.mox.VerifyAll()

  def testEmptyList(self):
    pkgindex = SimplePackageIndex()
    pristine = SimplePackageIndex()
    uploads = pkgindex.ResolveDuplicateUploads([])
    self.assertEqual(uploads, pkgindex.packages)
    self.assertEqual(len(pkgindex.packages), len(pristine.packages))
    for pkg1, pkg2 in zip(pkgindex.packages, pristine.packages):
      self.assertNotEqual(pkg1['MTIME'], pkg2['MTIME'])
      del pkg1['MTIME']
      del pkg2['MTIME']
    self.assertEqual(pkgindex.modified, False)

  def testEmptyIndex(self):
    pkgindex = SimplePackageIndex()
    pristine = SimplePackageIndex()
    empty = SimplePackageIndex(packages=False)
    uploads = pkgindex.ResolveDuplicateUploads([empty])
    self.assertEqual(uploads, pkgindex.packages)
    self.assertEqual(len(pkgindex.packages), len(pristine.packages))
    for pkg1, pkg2 in zip(pkgindex.packages, pristine.packages):
      self.assertNotEqual(pkg1['MTIME'], pkg2['MTIME'])
      del pkg1['MTIME']
      del pkg2['MTIME']
      self.assertEqual(pkg1, pkg2)
    self.assertEqual(pkgindex.modified, False)

  def testDuplicates(self):
    pkgindex = SimplePackageIndex()
    dup_pkgindex = SimplePackageIndex()
    expected_pkgindex = SimplePackageIndex()
    for pkg in expected_pkgindex.packages:
      pkg.setdefault('PATH', pkg['CPV'] + '.tbz2')
    pkgindex.ResolveDuplicateUploads([dup_pkgindex])
    self.assertEqual(pkgindex.packages, expected_pkgindex.packages)

  def testMissingSHA1(self):
    pkgindex = SimplePackageIndex()
    dup_pkgindex = SimplePackageIndex()
    expected_pkgindex = SimplePackageIndex()
    del pkgindex.packages[0]['SHA1']
    del expected_pkgindex.packages[0]['SHA1']
    for pkg in expected_pkgindex.packages[1:]:
      pkg.setdefault('PATH', pkg['CPV'] + '.tbz2')
    pkgindex.ResolveDuplicateUploads([dup_pkgindex])
    self.assertNotEqual(pkgindex.packages[0]['MTIME'],
                        expected_pkgindex.packages[0]['MTIME'])
    del pkgindex.packages[0]['MTIME']
    del expected_pkgindex.packages[0]['MTIME']
    self.assertEqual(pkgindex.packages, expected_pkgindex.packages)


class TestWritePackageIndex(unittest.TestCase):

  def setUp(self):
    self.mox = mox.Mox()

  def tearDown(self):
    self.mox.UnsetStubs()
    self.mox.VerifyAll()

  def testSimple(self):
    pkgindex = SimplePackageIndex()
    self.mox.StubOutWithMock(pkgindex, 'Write')
    pkgindex.Write(mox.IgnoreArg())
    self.mox.ReplayAll()
    f = pkgindex.WriteToNamedTemporaryFile()
    self.assertEqual(f.read(), '')


class TestUploadPrebuilt(unittest.TestCase):

  def setUp(self):
    class MockTemporaryFile(object):
      def __init__(self, name):
        self.name = name
    self.mox = mox.Mox()
    self.pkgindex = SimplePackageIndex()
    self.mox.StubOutWithMock(prebuilt, 'GrabLocalPackageIndex')
    prebuilt.GrabLocalPackageIndex('/packages').AndReturn(self.pkgindex)
    self.mox.StubOutWithMock(prebuilt, 'RemoteUpload')
    self.mox.StubOutWithMock(self.pkgindex, 'ResolveDuplicateUploads')
    self.pkgindex.ResolveDuplicateUploads([]).AndReturn(PRIVATE_PACKAGES)
    self.mox.StubOutWithMock(self.pkgindex, 'WriteToNamedTemporaryFile')
    fake_pkgs_file = MockTemporaryFile('fake')
    self.pkgindex.WriteToNamedTemporaryFile().AndReturn(fake_pkgs_file)

  def tearDown(self):
    self.mox.UnsetStubs()
    self.mox.VerifyAll()

  def doRsyncUpload(self, suffix):
    self.mox.StubOutWithMock(prebuilt, '_RetryRun')
    remote_path = '/dir/%s' % suffix.rstrip('/')
    full_remote_path = 'chromeos-prebuilt:%s' % remote_path
    cmds = [['ssh', 'chromeos-prebuilt', 'mkdir', '-p', remote_path],
            ['rsync', '-av', '--chmod=a+r', 'fake',
             full_remote_path + '/Packages'],
            ['rsync', '-Rav', 'private.tbz2', full_remote_path + '/']]
    for cmd in cmds:
      prebuilt._RetryRun(cmd, cwd='/packages').AndReturn(True)
    self.mox.ReplayAll()
    uri = self.pkgindex.header['URI']
    uploader = prebuilt.PrebuiltUploader('chromeos-prebuilt:/dir',
        'public-read', uri, [], '/', [], False, 'foo', False,
        ('x86-foo', 'aura'), [])
    uploader._UploadPrebuilt('/packages', suffix)

  def testSuccessfulGsUpload(self):
    uploads = {'/packages/private.tbz2': 'gs://foo/private.tbz2'}
    self.mox.StubOutWithMock(prebuilt, 'GenerateUploadDict')
    prebuilt.GenerateUploadDict('/packages', 'gs://foo/suffix',
        PRIVATE_PACKAGES).AndReturn(uploads)
    uploads = uploads.copy()
    uploads['fake'] = 'gs://foo/suffix/Packages'
    acl = 'public-read'
    prebuilt.RemoteUpload(acl, uploads).AndReturn([None])
    self.mox.ReplayAll()
    uri = self.pkgindex.header['URI']
    uploader = prebuilt.PrebuiltUploader('gs://foo', acl, uri, [], '/', [],
                                         False, 'foo', False, 'x86-foo', [])
    uploader._UploadPrebuilt('/packages', 'suffix')

  def testSuccessfulRsyncUploadWithNoTrailingSlash(self):
    self.doRsyncUpload('suffix')

  def testSuccessfulRsyncUploadWithTrailingSlash(self):
    self.doRsyncUpload('suffix/')


class TestSyncPrebuilts(unittest.TestCase):

  def setUp(self):
    self.mox = mox.Mox()
    self.mox.StubOutWithMock(prebuilt, 'DeterminePrebuiltConfFile')
    self.mox.StubOutWithMock(prebuilt, 'RevGitFile')
    self.mox.StubOutWithMock(prebuilt, 'UpdateBinhostConfFile')
    self.build_path = '/trunk'
    self.upload_location = 'gs://upload/'
    self.version = '1'
    self.binhost = 'http://prebuilt/'
    self.key = 'PORTAGE_BINHOST'
    self.mox.StubOutWithMock(prebuilt.PrebuiltUploader, '_UploadPrebuilt')

  def tearDown(self):
    self.mox.UnsetStubs()
    self.mox.VerifyAll()

  def testSyncHostPrebuilts(self):
    board = 'x86-foo'
    target = prebuilt.BuildTarget(board, 'aura')
    slave_targets = [prebuilt.BuildTarget('x86-bar', 'aura')]
    package_path = os.path.join(self.build_path,
                                prebuilt._HOST_PACKAGES_PATH)
    url_suffix = prebuilt._REL_HOST_PATH % {'version': self.version,
        'host_arch': prebuilt._HOST_ARCH, 'target': target}
    packages_url_suffix = '%s/packages' % url_suffix.rstrip('/')
    prebuilt.PrebuiltUploader._UploadPrebuilt(package_path,
        packages_url_suffix).AndReturn(True)
    url_value = '%s/%s/' % (self.binhost.rstrip('/'),
                            packages_url_suffix.rstrip('/'))
    urls = [url_value.replace('foo', 'bar'), url_value]
    binhost = ' '.join(urls)
    prebuilt.RevGitFile(mox.IgnoreArg(), binhost, key=self.key, dryrun=False)
    prebuilt.UpdateBinhostConfFile(mox.IgnoreArg(), self.key, binhost)
    self.mox.ReplayAll()
    uploader = prebuilt.PrebuiltUploader(
        self.upload_location, 'public-read', self.binhost, [],
        self.build_path, [], False, 'foo', False, target, slave_targets)
    uploader._SyncHostPrebuilts(self.version, self.key, True, True)

  def testSyncBoardPrebuilts(self):
    board = 'x86-foo'
    target = prebuilt.BuildTarget(board, 'aura')
    slave_targets = [prebuilt.BuildTarget('x86-bar', 'aura')]
    board_path = os.path.join(self.build_path,
        prebuilt._BOARD_PATH % {'board': board})
    package_path = os.path.join(board_path, 'packages')
    url_suffix = prebuilt._REL_BOARD_PATH % {'version': self.version,
        'target': target}
    packages_url_suffix = '%s/packages' % url_suffix.rstrip('/')
    self.mox.StubOutWithMock(multiprocessing.Process, '__init__')
    self.mox.StubOutWithMock(multiprocessing.Process, 'exitcode')
    self.mox.StubOutWithMock(multiprocessing.Process, 'start')
    self.mox.StubOutWithMock(multiprocessing.Process, 'join')
    multiprocessing.Process.__init__(target=mox.IgnoreArg(),
        args=(board_path, url_suffix, self.version))
    multiprocessing.Process.start()
    prebuilt.PrebuiltUploader._UploadPrebuilt(package_path,
        packages_url_suffix).AndReturn(True)
    multiprocessing.Process.join()
    multiprocessing.Process.exitcode = 0
    url_value = '%s/%s/' % (self.binhost.rstrip('/'),
                            packages_url_suffix.rstrip('/'))
    bar_binhost = url_value.replace('foo', 'bar')
    prebuilt.DeterminePrebuiltConfFile(self.build_path,
        slave_targets[0]).AndReturn('bar')
    prebuilt.RevGitFile('bar', bar_binhost, key=self.key, dryrun=False)
    prebuilt.UpdateBinhostConfFile(mox.IgnoreArg(), self.key, bar_binhost)
    prebuilt.DeterminePrebuiltConfFile(self.build_path, target).AndReturn('foo')
    prebuilt.RevGitFile('foo', url_value, key=self.key, dryrun=False)
    prebuilt.UpdateBinhostConfFile(mox.IgnoreArg(), self.key, url_value)
    self.mox.ReplayAll()
    uploader = prebuilt.PrebuiltUploader(
        self.upload_location, 'public-read', self.binhost, [],
        self.build_path, [], False, 'foo', False, target, slave_targets)
    uploader._SyncBoardPrebuilts(self.version, self.key, True, True, True)


class TestMain(unittest.TestCase):

  def setUp(self):
    self.mox = mox.Mox()

  def tearDown(self):
    self.mox.UnsetStubs()
    self.mox.VerifyAll()

  def testMain(self):
    """Test that the main function works."""
    options = mox.MockObject(object)
    old_binhost = 'http://prebuilt/1'
    options.previous_binhost_url = [old_binhost]
    options.board = 'x86-foo'
    options.profile = None
    options.target = prebuilt.BuildTarget(options.board, options.profile)
    options.build_path = '/trunk'
    options.debug = False
    options.private = True
    options.packages = []
    options.sync_host = True
    options.git_sync = True
    options.upload_board_tarball = True
    options.upload = 'gs://upload/'
    options.binhost_base_url = options.upload
    options.prepend_version = True
    options.set_version = None
    options.skip_upload = False
    options.filters = True
    options.key = 'PORTAGE_BINHOST'
    options.binhost_conf_dir = 'foo'
    options.sync_binhost_conf = True
    options.slave_targets = [prebuilt.BuildTarget('x86-bar', 'aura')]
    self.mox.StubOutWithMock(prebuilt, 'ParseOptions')
    prebuilt.ParseOptions().AndReturn(options)
    self.mox.StubOutWithMock(prebuilt, 'GrabRemotePackageIndex')
    prebuilt.GrabRemotePackageIndex(old_binhost).AndReturn(True)
    self.mox.StubOutWithMock(prebuilt.PrebuiltUploader, '__init__')
    self.mox.StubOutWithMock(prebuilt, 'GetBoardPathFromCrosOverlayList')
    fake_overlay_path = '/fake_path'
    prebuilt.GetBoardPathFromCrosOverlayList(
        options.build_path, options.board).AndReturn(fake_overlay_path)
    expected_gs_acl_path = os.path.join(fake_overlay_path,
                                        prebuilt._GOOGLESTORAGE_ACL_FILE)
    prebuilt.PrebuiltUploader.__init__(options.upload, expected_gs_acl_path,
                                       options.upload, mox.IgnoreArg(),
                                       options.build_path, options.packages,
                                       False, options.binhost_conf_dir, False,
                                       options.target, options.slave_targets)
    self.mox.StubOutWithMock(prebuilt.PrebuiltUploader, '_SyncHostPrebuilts')
    prebuilt.PrebuiltUploader._SyncHostPrebuilts(mox.IgnoreArg(), options.key,
        options.git_sync, options.sync_binhost_conf)
    self.mox.StubOutWithMock(prebuilt.PrebuiltUploader, '_SyncBoardPrebuilts')
    prebuilt.PrebuiltUploader._SyncBoardPrebuilts(
        mox.IgnoreArg(), options.key, options.git_sync,
        options.sync_binhost_conf, options.upload_board_tarball)
    self.mox.ReplayAll()
    prebuilt.main()

if __name__ == '__main__':
  unittest.main()
