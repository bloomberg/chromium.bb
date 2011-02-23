#!/usr/bin/python
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import mox
import os
import shutil
import sys
import tempfile
import unittest
import urllib

import constants
sys.path.append(constants.SOURCE_ROOT)
import prebuilt
from chromite.lib import cros_build_lib
from chromite.lib.binpkg import PackageIndex

PUBLIC_PACKAGES = [{'CPV': 'gtk+/public1', 'SHA1': '1'},
                   {'CPV': 'gtk+/public2', 'SHA1': '2',
                    'PATH': 'gtk+/foo.tgz'}]
PRIVATE_PACKAGES = [{'CPV': 'private', 'SHA1': '3'}]


def SimplePackageIndex(header=True, packages=True):
   pkgindex = PackageIndex()
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
    current_version_str = self._read_version_file()
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


class TestPrebuiltFilters(unittest.TestCase):

  def setUp(self):
    self.tmp_dir = tempfile.mkdtemp()
    self.private_dir = os.path.join(self.tmp_dir,
                                    prebuilt._PRIVATE_OVERLAY_DIR)
    self.private_structure_base = 'chromeos-overlay/chromeos-base'
    self.private_pkgs = ['test-package/salt-flavor-0.1.r3.ebuild',
                         'easy/alpha_beta-0.1.41.r3.ebuild',
                         'dev/j-t-r-0.1.r3.ebuild', ]
    self.expected_filters = set(['salt-flavor', 'alpha_beta', 'j-t-r'])

  def tearDown(self):
    if self.tmp_dir:
      shutil.rmtree(self.tmp_dir)

  def _CreateNestedDir(self, tmp_dir, dir_structure):
    for entry in dir_structure:
      full_path = os.path.join(os.path.join(tmp_dir, entry))
      # ensure dirs are created
      try:
        os.makedirs(os.path.dirname(full_path))
        if full_path.endswith('/'):
          # we only want to create directories
          return
      except OSError, err:
        if err.errno == errno.EEXIST:
          # we don't care if the dir already exists
          pass
        else:
          raise
      # create dummy files
      tmp = open(full_path, 'w')
      tmp.close()

  def _LoadPrivateMockFilters(self):
    """Load mock filters as defined in the setUp function."""
    dir_structure = [os.path.join(self.private_structure_base, entry)
                     for entry in self.private_pkgs]

    self._CreateNestedDir(self.private_dir, dir_structure)
    prebuilt.LoadPrivateFilters(self.tmp_dir)

  def testFilterPattern(self):
    """Check that particular packages are filtered properly."""
    self._LoadPrivateMockFilters()
    packages = ['/some/dir/area/j-t-r-0.1.r3.tbz',
                '/var/pkgs/new/alpha_beta-0.2.3.4.tbz',
                '/usr/local/cache/good-0.1.3.tbz',
                '/usr-blah/b_d/salt-flavor-0.0.3.tbz']
    expected_list = ['/usr/local/cache/good-0.1.3.tbz']
    filtered_list = [file for file in packages if not
                     prebuilt.ShouldFilterPackage(file)]
    self.assertEqual(expected_list, filtered_list)

  def testLoadPrivateFilters(self):
    self._LoadPrivateMockFilters()
    prebuilt.LoadPrivateFilters(self.tmp_dir)
    self.assertEqual(self.expected_filters, prebuilt._FILTER_PACKAGES)

  def testEmptyFiltersErrors(self):
    """Ensure LoadPrivateFilters errors if an empty list is generated."""
    os.makedirs(os.path.join(self.tmp_dir, prebuilt._PRIVATE_OVERLAY_DIR))
    self.assertRaises(prebuilt.FiltersEmpty, prebuilt.LoadPrivateFilters,
                      self.tmp_dir)


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
    files = {'test': '/uasd'}
    result = prebuilt.RemoteUpload('public-read', files)
    self.assertEqual(result, set([('test', '/uasd')]))

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
      self.assertEqual(
        prebuilt.DeterminePrebuiltConfFile(fake_path, target),
        expected_results['result'])

  def testDeterminePrebuiltConfGarbage(self):
    """Ensure an exception is raised on bad input."""
    self.assertRaises(prebuilt.UnknownBoardFormat,
                      prebuilt.DeterminePrebuiltConfFile,
                      'fake_path', 'asdfasdf')


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
    pkgindex._PopulateDuplicateDB(db)
    self.assertEqual(db, {})

  def testNormalIndex(self):
    pkgindex = SimplePackageIndex()
    db = {}
    pkgindex._PopulateDuplicateDB(db)
    self.assertEqual(len(db), 3)
    self.assertEqual(db['1'], 'http://www.example.com/gtk+/public1.tbz2')
    self.assertEqual(db['2'], 'http://www.example.com/gtk+/foo.tgz')
    self.assertEqual(db['3'], 'http://www.example.com/private.tbz2')

  def testMissingSHA1(self):
    db = {}
    pkgindex = SimplePackageIndex()
    del pkgindex.packages[0]['SHA1']
    pkgindex._PopulateDuplicateDB(db)
    self.assertEqual(len(db), 2)
    self.assertEqual(db['2'], 'http://www.example.com/gtk+/foo.tgz')
    self.assertEqual(db['3'], 'http://www.example.com/private.tbz2')

  def testFailedPopulate(self):
    db = {}
    pkgindex = SimplePackageIndex(header=False)
    self.assertRaises(KeyError, pkgindex._PopulateDuplicateDB, db)
    pkgindex = SimplePackageIndex()
    del pkgindex.packages[0]['CPV']
    self.assertRaises(KeyError, pkgindex._PopulateDuplicateDB, db)


class TestResolveDuplicateUploads(unittest.TestCase):

  def testEmptyList(self):
    pkgindex = SimplePackageIndex()
    pristine = SimplePackageIndex()
    uploads = pkgindex.ResolveDuplicateUploads([])
    self.assertEqual(uploads, pristine.packages)
    self.assertEqual(pkgindex.packages, pristine.packages)
    self.assertEqual(pkgindex.modified, False)

  def testEmptyIndex(self):
    pkgindex = SimplePackageIndex()
    pristine = SimplePackageIndex()
    empty = SimplePackageIndex(packages=False)
    uploads = pkgindex.ResolveDuplicateUploads([empty])
    self.assertEqual(uploads, pristine.packages)
    self.assertEqual(pkgindex.packages, pristine.packages)
    self.assertEqual(pkgindex.modified, False)

  def testDuplicates(self):
    pkgindex = SimplePackageIndex()
    dup_pkgindex = SimplePackageIndex()
    expected_pkgindex = SimplePackageIndex()
    for pkg in expected_pkgindex.packages:
      pkg.setdefault('PATH', pkg['CPV'] + '.tbz2')
    uploads = pkgindex.ResolveDuplicateUploads([dup_pkgindex])
    self.assertEqual(pkgindex.packages, expected_pkgindex.packages)

  def testMissingSHA1(self):
    db = {}
    pkgindex = SimplePackageIndex()
    dup_pkgindex = SimplePackageIndex()
    expected_pkgindex = SimplePackageIndex()
    del pkgindex.packages[0]['SHA1']
    del expected_pkgindex.packages[0]['SHA1']
    for pkg in expected_pkgindex.packages[1:]:
      pkg.setdefault('PATH', pkg['CPV'] + '.tbz2')
    uploads = pkgindex.ResolveDuplicateUploads([dup_pkgindex])
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
    cmds = ['ssh chromeos-prebuilt mkdir -p %s' % remote_path,
            'rsync -av --chmod=a+r fake %s/Packages' % full_remote_path,
            'rsync -Rav private.tbz2 %s/' % full_remote_path]
    for cmd in cmds:
      prebuilt._RetryRun(cmd, shell=True, cwd='/packages').AndReturn(True)
    self.mox.ReplayAll()
    uri = self.pkgindex.header['URI']
    uploader = prebuilt.PrebuiltUploader('chromeos-prebuilt:/dir',
        'public-read', uri, [])
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
    uploader = prebuilt.PrebuiltUploader('gs://foo', acl, uri, [])
    uploader._UploadPrebuilt('/packages', 'suffix')

  def testSuccessfulRsyncUploadWithNoTrailingSlash(self):
    self.doRsyncUpload('suffix')

  def testSuccessfulRsyncUploadWithTrailingSlash(self):
    self.doRsyncUpload('suffix/')


class TestSyncPrebuilts(unittest.TestCase):

  def setUp(self):
    self.mox = mox.Mox()
    self.mox.StubOutWithMock(prebuilt, '_GetCrossCompilerSubdirectories')
    self.mox.StubOutWithMock(prebuilt, 'DeterminePrebuiltConfFile')
    self.mox.StubOutWithMock(prebuilt, 'RevGitFile')
    self.mox.StubOutWithMock(prebuilt, 'UpdateBinhostConfFile')
    self.build_path = '/trunk'
    self.upload_location = 'gs://upload/'
    self.version = '1'
    self.binhost = 'http://prebuilt/'
    self.key = 'PORTAGE_BINHOST'
    self.uploader = prebuilt.PrebuiltUploader(self.upload_location,
        'public-read', self.binhost, [])
    self.mox.StubOutWithMock(self.uploader, '_UploadPrebuilt')

  def tearDown(self):
    self.mox.UnsetStubs()
    self.mox.VerifyAll()

  def testSyncHostPrebuilts(self):
    package_path = os.path.join(self.build_path,
                                prebuilt._HOST_PACKAGES_PATH)
    url_suffix = prebuilt._REL_HOST_PATH % {'version': self.version,
        'target': prebuilt._HOST_TARGET }
    subdir = 'cross/x86'
    prebuilt._GetCrossCompilerSubdirectories(self.build_path).AndReturn(
        [subdir])
    cross_package_path = os.path.join(package_path, subdir)
    cross_url_suffix = '%s/%s' % (url_suffix.rstrip('/'), subdir)
    self.uploader._UploadPrebuilt(cross_package_path, cross_url_suffix)
    self.uploader._UploadPrebuilt(package_path, url_suffix)
    url_value = '%s/%s/' % (self.binhost.rstrip('/'), url_suffix.rstrip('/'))
    prebuilt.RevGitFile(mox.IgnoreArg(), url_value, key=self.key)
    prebuilt.UpdateBinhostConfFile(mox.IgnoreArg(), self.key, url_value)
    self.mox.ReplayAll()
    self.uploader._SyncHostPrebuilts(self.build_path, self.version, self.key,
        True, True)

  def testSyncBoardPrebuilts(self):
    board = 'x86-generic'
    board_path = os.path.join(self.build_path,
        prebuilt._BOARD_PATH % {'board': board})
    package_path = os.path.join(board_path, 'packages')
    url_suffix = prebuilt._REL_BOARD_PATH % {'version': self.version,
        'board': board }
    self.uploader._UploadPrebuilt(package_path, url_suffix)
    url_value = '%s/%s/' % (self.binhost.rstrip('/'), url_suffix.rstrip('/'))
    prebuilt.DeterminePrebuiltConfFile(self.build_path, board).AndReturn('foo')
    prebuilt.RevGitFile('foo', url_value, key=self.key)
    prebuilt.UpdateBinhostConfFile(mox.IgnoreArg(), self.key, url_value)
    self.mox.ReplayAll()
    self.uploader._SyncBoardPrebuilts(board, self.build_path, self.version,
        self.key, True, True)


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
    options.board = 'x86-generic'
    options.build_path = '/trunk'
    options.private = True
    options.sync_host = True
    options.git_sync = True
    options.upload = 'gs://upload/'
    options.binhost_base_url = options.upload
    options.prepend_version = True
    options.filters = True
    options.key = 'PORTAGE_BINHOST'
    options.sync_binhost_conf = True
    self.mox.StubOutWithMock(prebuilt, 'ParseOptions')
    prebuilt.ParseOptions().AndReturn(options)
    self.mox.StubOutWithMock(prebuilt, 'LoadPrivateFilters')
    prebuilt.LoadPrivateFilters(options.build_path)
    self.mox.StubOutWithMock(prebuilt, '_GetCrossCompilerSubdirectories')
    prebuilt._GetCrossCompilerSubdirectories(
        options.build_path).MultipleTimes(2).AndReturn(['subdir'])
    self.mox.StubOutWithMock(prebuilt, 'GrabRemotePackageIndex')
    prebuilt.GrabRemotePackageIndex(old_binhost).AndReturn(True)
    prebuilt.GrabRemotePackageIndex(old_binhost + '/subdir').AndReturn(True)
    self.mox.StubOutWithMock(prebuilt.PrebuiltUploader, '__init__')
    prebuilt.PrebuiltUploader.__init__(options.upload, 'private',
        options.binhost_base_url, mox.IgnoreArg())
    self.mox.StubOutWithMock(prebuilt.PrebuiltUploader, '_SyncHostPrebuilts')
    prebuilt.PrebuiltUploader._SyncHostPrebuilts(options.build_path,
        mox.IgnoreArg(), options.key, options.git_sync,
        options.sync_binhost_conf)
    self.mox.StubOutWithMock(prebuilt.PrebuiltUploader, '_SyncBoardPrebuilts')
    prebuilt.PrebuiltUploader._SyncBoardPrebuilts(options.board,
        options.build_path, mox.IgnoreArg(), options.key, options.git_sync,
        options.sync_binhost_conf)
    self.mox.ReplayAll()
    prebuilt.main()

if __name__ == '__main__':
  unittest.main()
