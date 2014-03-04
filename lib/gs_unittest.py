#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the gs.py module."""

import functools
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import retry_util

# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock


def PatchGS(*args, **kwargs):
  """Convenience method for patching GSContext."""
  return mock.patch.object(gs.GSContext, *args, **kwargs)


class GSContextMock(partial_mock.PartialCmdMock):
  """Used to mock out the GSContext class."""
  TARGET = 'chromite.lib.gs.GSContext'
  ATTRS = ('__init__', 'DoCommand', 'DEFAULT_SLEEP_TIME',
           'DEFAULT_RETRIES', 'DEFAULT_BOTO_FILE', 'DEFAULT_GSUTIL_BIN',
           'DEFAULT_GSUTIL_BUILDER_BIN', 'GSUTIL_URL')
  DEFAULT_ATTR = 'DoCommand'

  GSResponsePreconditionFailed = """
[Setting Content-Type=text/x-python]
        GSResponseError:: status=412, code=PreconditionFailed,
        reason=Precondition Failed."""

  DEFAULT_SLEEP_TIME = 0
  DEFAULT_RETRIES = 2
  TMP_ROOT = '/tmp/cros_unittest'
  DEFAULT_BOTO_FILE = '%s/boto_file' % TMP_ROOT
  DEFAULT_GSUTIL_BIN = '%s/gsutil_bin' % TMP_ROOT
  DEFAULT_GSUTIL_BUILDER_BIN = DEFAULT_GSUTIL_BIN
  GSUTIL_URL = None

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self, create_tempdir=True)

  def _SetGSUtilUrl(self):
    tempfile = os.path.join(self.tempdir, 'tempfile')
    osutils.WriteFile(tempfile, 'some content')
    gsutil_path = os.path.join(self.tempdir, gs.GSContext.GSUTIL_TAR)
    cros_build_lib.CreateTarball(gsutil_path, self.tempdir, inputs=[tempfile])
    self.GSUTIL_URL = 'file://%s' % gsutil_path

  def PreStart(self):
    os.environ.pop("BOTO_CONFIG", None)
    # Set it here for now, instead of mocking out Cached() directly because
    # python-mock has a bug with mocking out class methods with autospec=True.
    # TODO(rcui): Change this when this is fixed in PartialMock.
    self._SetGSUtilUrl()

  def _target__init__(self, *args, **kwargs):
    with PatchGS('_CheckFile', return_value=True):
      self.backup['__init__'](*args, **kwargs)

  def DoCommand(self, inst, gsutil_cmd, **kwargs):
    result = self._results['DoCommand'].LookupResult(
        (gsutil_cmd,), hook_args=(inst, gsutil_cmd,), hook_kwargs=kwargs)

    rc_mock = cros_build_lib_unittest.RunCommandMock()
    rc_mock.AddCmdResult(
        partial_mock.ListRegex('gsutil'), result.returncode, result.output,
        result.error)

    with rc_mock:
      return self.backup['DoCommand'](inst, gsutil_cmd, **kwargs)


class AbstractGSContextTest(cros_test_lib.MockTempDirTestCase):
  """Base class for GSContext tests."""

  def setUp(self):
    self.gs_mock = self.StartPatcher(GSContextMock())
    self.gs_mock.SetDefaultCmdResult()
    self.ctx = gs.GSContext()


class CanonicalizeURLTest(cros_test_lib.TestCase):
  """Tests for the CanonicalizeURL function."""

  def _checkit(self, in_url, exp_url):
    self.assertEqual(gs.CanonicalizeURL(in_url), exp_url)

  def testPublicUrl(self):
    """Test public https URLs."""
    self._checkit(
        'https://commondatastorage.googleapis.com/releases/some/file/t.gz',
        'gs://releases/some/file/t.gz')

  def testPrivateUrl(self):
    """Test private https URLs."""
    self._checkit(
        'https://storage.cloud.google.com/releases/some/file/t.gz',
        'gs://releases/some/file/t.gz')

  def testDuplicateBase(self):
    """Test multiple prefixes in a single URL."""
    self._checkit(
        ('https://storage.cloud.google.com/releases/some/'
         'https://storage.cloud.google.com/some/file/t.gz'),
        ('gs://releases/some/'
         'https://storage.cloud.google.com/some/file/t.gz'))


class VersionTest(AbstractGSContextTest):
  """Tests GSContext.gsutil_version functionality."""

  LOCAL_PATH = '/tmp/file'
  GIVEN_REMOTE = EXPECTED_REMOTE = 'gs://test/path/file'

  def testGetVersionStdout(self):
    """Simple gsutil_version fetch test from stdout."""
    self.gs_mock.AddCmdResult(partial_mock.In('version'), returncode=0,
                              output='gsutil version 3.35\n')
    self.assertEquals('3.35', self.ctx.gsutil_version)

  def testGetVersionStderr(self):
    """Simple gsutil_version fetch test from stderr."""
    self.gs_mock.AddCmdResult(partial_mock.In('version'), returncode=0,
                              error='gsutil version 3.36\n')
    self.assertEquals('3.36', self.ctx.gsutil_version)

  def testGetVersionCached(self):
    """Simple gsutil_version fetch test from cache."""
    self.ctx._gsutil_version = '3.37'
    self.assertEquals('3.37', self.ctx.gsutil_version)

  def testGetVersionBadOutput(self):
    """Simple gsutil_version fetch test from cache."""
    self.gs_mock.AddCmdResult(partial_mock.In('version'), returncode=0,
                              output='gobblety gook\n')
    self.assertRaises(gs.GSContextException, getattr, self.ctx,
                      'gsutil_version')


class CopyTest(AbstractGSContextTest):
  """Tests GSContext.Copy() functionality."""

  LOCAL_PATH = '/tmp/file'
  GIVEN_REMOTE = EXPECTED_REMOTE = 'gs://test/path/file'
  ACL = 'public-read'

  def _Copy(self, ctx, src, dst, **kwargs):
    return ctx.Copy(src, dst, **kwargs)

  def Copy(self, ctx=None, **kwargs):
    if ctx is None:
      ctx = self.ctx
    return self._Copy(ctx, self.LOCAL_PATH, self.GIVEN_REMOTE, **kwargs)

  def testBasic(self):
    """Simple copy test."""
    self.Copy()
    self.gs_mock.assertCommandContains(
        ['cp', '--', self.LOCAL_PATH, self.EXPECTED_REMOTE])

  def testWithACL(self):
    """ACL specified during init."""
    ctx = gs.GSContext(acl=self.ACL)
    self.Copy(ctx=ctx)
    self.gs_mock.assertCommandContains(['cp', '-a', self.ACL])

  def testWithACL2(self):
    """ACL specified during invocation."""
    self.Copy(acl=self.ACL)
    self.gs_mock.assertCommandContains(['cp', '-a', self.ACL])

  def testWithACL3(self):
    """ACL specified during invocation that overrides init."""
    ctx = gs.GSContext(acl=self.ACL)
    self.Copy(ctx=ctx, acl=self.ACL)
    self.gs_mock.assertCommandContains(['cp', '-a', self.ACL])

  def testRunCommandError(self):
    """Test RunCommandError is propagated."""
    self.gs_mock.AddCmdResult(partial_mock.In('cp'), returncode=1)
    self.assertRaises(cros_build_lib.RunCommandError, self.Copy)

  def testGSContextException(self):
    """GSContextException is raised properly."""
    self.gs_mock.AddCmdResult(
        partial_mock.In('cp'), returncode=1,
        error=self.gs_mock.GSResponsePreconditionFailed)
    self.assertRaises(gs.GSContextException, self.Copy)


class CopyIntoTest(CopyTest):
  """Test CopyInto functionality."""

  FILE = 'ooga'
  GIVEN_REMOTE = 'gs://test/path/file'
  EXPECTED_REMOTE = '%s/%s' % (GIVEN_REMOTE, FILE)

  def _Copy(self, ctx, *args, **kwargs):
    return ctx.CopyInto(*args, filename=self.FILE, **kwargs)


#pylint: disable=E1101,W0212
class GSContextInitTest(cros_test_lib.MockTempDirTestCase):
  """Tests GSContext.__init__() functionality."""

  def setUp(self):
    os.environ.pop("BOTO_CONFIG", None)
    self.bad_path = os.path.join(self.tempdir, 'nonexistent')

    file_list = ['gsutil_bin', 'boto_file', 'acl_file']
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, file_list)
    for f in file_list:
      setattr(self, f, os.path.join(self.tempdir, f))
    self.StartPatcher(PatchGS('DEFAULT_BOTO_FILE', new=self.boto_file))
    self.StartPatcher(PatchGS('DEFAULT_GSUTIL_BIN', new=self.gsutil_bin))

  def testInitGsutilBin(self):
    """Test we use the given gsutil binary, erroring where appropriate."""
    self.assertEquals(gs.GSContext().gsutil_bin, self.gsutil_bin)
    self.assertRaises(gs.GSContextException,
                      gs.GSContext, gsutil_bin=self.bad_path)

  def testBadGSUtilBin(self):
    """Test exception thrown for bad gsutil paths."""
    self.assertRaises(gs.GSContextException, gs.GSContext,
                      gsutil_bin=self.bad_path)

  def testInitBotoFileEnv(self):
    os.environ['BOTO_CONFIG'] = self.gsutil_bin
    self.assertTrue(gs.GSContext().boto_file, self.gsutil_bin)
    self.assertEqual(gs.GSContext(boto_file=self.acl_file).boto_file,
                     self.acl_file)
    self.assertRaises(gs.GSContextException, gs.GSContext,
                      boto_file=self.bad_path)

  def testInitBotoFileEnvError(self):
    """Boto file through env var error."""
    self.assertEquals(gs.GSContext().boto_file, self.boto_file)
    # Check env usage next; no need to cleanup, teardown handles it,
    # and we want the env var to persist for the next part of this test.
    os.environ['BOTO_CONFIG'] = self.bad_path
    self.assertRaises(gs.GSContextException, gs.GSContext)

  def testInitBotoFileError(self):
    """Test bad boto file."""
    self.assertRaises(gs.GSContextException, gs.GSContext,
                      boto_file=self.bad_path)

  def testInitAclFile(self):
    """Test ACL selection logic in __init__."""
    self.assertEqual(gs.GSContext().acl, None)
    self.assertEqual(gs.GSContext(acl=self.acl_file).acl,
                     self.acl_file)

  def _testHTTPProxySettings(self, d):
    flags = gs.GSContext().gsutil_flags
    for key in d:
      flag = 'Boto:%s=%s' % (key, d[key])
      error_msg = '%s not in %s' % (flag, ' '.join(flags))
      self.assertTrue(flag in flags, error_msg)

  def testHTTPProxy(self):
    """Test we set http proxy correctly."""
    d = {'proxy': 'fooserver', 'proxy_user': 'foouser',
         'proxy_pass': 'foopasswd', 'proxy_port': '8080'}
    os.environ['http_proxy'] = 'http://%s:%s@%s:%s/' % (
        d['proxy_user'], d['proxy_pass'], d['proxy'], d['proxy_port'])
    self._testHTTPProxySettings(d)

  def testHTTPProxyNoPort(self):
    """Test we accept http proxy without port number."""
    d = {'proxy': 'fooserver', 'proxy_user': 'foouser',
         'proxy_pass': 'foopasswd'}
    os.environ['http_proxy'] = 'http://%s:%s@%s/' % (
        d['proxy_user'], d['proxy_pass'], d['proxy'])
    self._testHTTPProxySettings(d)

  def testHTTPProxyNoUserPasswd(self):
    """Test we accept http proxy without user and password."""
    d = {'proxy': 'fooserver', 'proxy_port': '8080'}
    os.environ['http_proxy'] = 'http://%s:%s/' % (d['proxy'], d['proxy_port'])
    self._testHTTPProxySettings(d)

  def testHTTPProxyNoPasswd(self):
    """Test we accept http proxy without password."""
    d = {'proxy': 'fooserver', 'proxy_user': 'foouser',
         'proxy_port': '8080'}
    os.environ['http_proxy'] = 'http://%s@%s:%s/' % (
        d['proxy_user'], d['proxy'], d['proxy_port'])
    self._testHTTPProxySettings(d)


class GSDoCommandTest(cros_test_lib.TestCase):
  """Tests of gs.DoCommand behavior.

  This test class inherits from cros_test_lib.TestCase instead of from
  AbstractGSContextTest, because the latter unnecessarily mocks out
  cros_build_lib.RunCommand, in a way that breaks _testDoCommand (changing
  cros_build_lib.RunCommand to refer to a mock instance after the
  GenericRetry mock has already been set up to expect a reference to the
  original RunCommand).
  """

  def setUp(self):
    self.ctx = gs.GSContext()

  def _testDoCommand(self, ctx, headers=(), retries=None, sleep=None,
                     version=None):
    if retries is None:
      retries = ctx.DEFAULT_RETRIES
    if sleep is None:
      sleep = ctx.DEFAULT_SLEEP_TIME

    with mock.patch.object(retry_util, 'GenericRetry', autospec=True):
      ctx.Copy('/blah', 'gs://foon', version=version)
      cmd = [self.ctx.gsutil_bin] + self.ctx.gsutil_flags + list(headers)
      cmd.append('-m')
      cmd += ['cp', '-r', '--', '/blah', 'gs://foon']

      retry_util.GenericRetry.assert_called_once_with(
          ctx._RetryFilter, retries,
          cros_build_lib.RunCommand,
          cmd, sleep=sleep,
          redirect_stderr=True,
          extra_env={'BOTO_CONFIG': mock.ANY})

  def testDoCommandDefault(self):
    """Verify the internal DoCommand function works correctly."""
    self._testDoCommand(self.ctx)

  def testDoCommandCustom(self):
    """Test that retries and sleep parameters are honored."""
    ctx = gs.GSContext(retries=4, sleep=1)
    self._testDoCommand(ctx, retries=4, sleep=1)

  def testVersion(self):
    """Test that the version field expands into the header."""
    self._testDoCommand(self.ctx, version=3,
                        headers=['-h', 'x-goog-if-generation-match:3'])


class GSRetryFilterTest(cros_test_lib.TestCase):
  """Verifies that we filter and process gsutil errors correctly."""

  LOCAL_PATH = '/tmp/file'
  REMOTE_PATH = ('gs://chromeos-prebuilt/board/beltino/paladin-R33-4926.0.0'
                 '-rc2/packages/chromeos-base/autotest-tests-0.0.1-r4679.tbz2')
  GSUTIL_TRACKER_DIR = '/foo'
  UPLOAD_TRACKER_FILE = (
      'upload_TRACKER_e0cae4d515c757f78a6d1f44a37d2f2ee236f3b8.1-r4679.tbz2.url'
      )
  DOWNLOAD_TRACKER_FILE = (
      'download_TRACKER_1e6cb2935097b207b634d86f91584d0b66de354e.___tmp_file.'
      'etag')
  RETURN_CODE = 3

  def setUp(self):
    self.ctx = gs.GSContext()
    self.ctx.DEFAULT_GSUTIL_TRACKER_DIR = self.GSUTIL_TRACKER_DIR

  def _getException(self, cmd, error, returncode=RETURN_CODE):
    result = cros_build_lib.CommandResult(
        error=error,
        cmd=cmd,
        returncode=returncode)
    return cros_build_lib.RunCommandError('blah', result)

  def assertNoSuchKey(self, error_msg):
    cmd = ['gsutil', 'ls', self.REMOTE_PATH]
    e = self._getException(cmd, error_msg)
    self.assertRaises(gs.GSNoSuchKey, self.ctx._RetryFilter, e)

  def assertPreconditionFailed(self, error_msg):
    cmd = ['gsutil', 'ls', self.REMOTE_PATH]
    e = self._getException(cmd, error_msg)
    self.assertRaises(gs.GSContextPreconditionFailed,
                        self.ctx._RetryFilter, e)

  def testRetryOnlyFlakyErrors(self):
    """Test that we retry only flaky errors."""
    cmd = ['gsutil', 'ls', self.REMOTE_PATH]
    e = self._getException(cmd, 'GSResponseError: status=502')
    self.assertTrue(self.ctx._RetryFilter(e))

    e = self._getException(cmd, 'GSResponseError: status=603')
    self.assertFalse(self.ctx._RetryFilter(e))

  def testRaiseGSErrors(self):
    """Test that we raise appropriate exceptions."""
    self.assertNoSuchKey('GSResponseError: status=404, code=NoSuchKey')
    self.assertNoSuchKey('InvalidUriError: Unrecognized scheme "http".')
    self.assertNoSuchKey('CommandException: No URIs matched.')
    self.assertPreconditionFailed(
        'GSResponseError: code=PreconditionFailed')

  @mock.patch('chromite.lib.osutils.SafeUnlink')
  @mock.patch('chromite.lib.osutils.ReadFile')
  @mock.patch('os.path.exists')
  def testRemoveUploadTrackerFile(self, exists_mock, readfile_mock,
                                  unlink_mock):
    """Test removal of tracker files for resumable upload failures."""
    cmd = ['gsutil', 'cp', self.LOCAL_PATH, self.REMOTE_PATH]
    e = self._getException(cmd, self.ctx.RESUMABLE_UPLOAD_ERROR)
    exists_mock.return_value = True
    readfile_mock.return_value = 'foohash'
    self.ctx._RetryFilter(e)
    tracker_file_path = os.path.join(self.GSUTIL_TRACKER_DIR,
                                     self.UPLOAD_TRACKER_FILE)
    unlink_mock.assert_called_once_with(tracker_file_path)

  @mock.patch('chromite.lib.osutils.SafeUnlink')
  @mock.patch('chromite.lib.osutils.ReadFile')
  @mock.patch('os.path.exists')
  def testRemoveDownloadTrackerFile(self, exists_mock, readfile_mock,
                                    unlink_mock):
    """Test removal of tracker files for resumable download failures."""
    cmd = ['gsutil', 'cp', self.REMOTE_PATH, self.LOCAL_PATH]
    e = self._getException(cmd, self.ctx.RESUMABLE_DOWNLOAD_ERROR)
    exists_mock.return_value = True
    readfile_mock.return_value = 'foohash'
    self.ctx._RetryFilter(e)
    tracker_file_path = os.path.join(self.GSUTIL_TRACKER_DIR,
                                     self.DOWNLOAD_TRACKER_FILE)
    unlink_mock.assert_called_once_with(tracker_file_path)

  def testRemoveTrackerFileOnlyForCP(self):
    """Test that we remove tracker files only for 'gsutil cp'."""
    cmd = ['gsutil', 'ls', self.REMOTE_PATH]
    e = self._getException(cmd, self.ctx.RESUMABLE_DOWNLOAD_ERROR)

    with mock.MagicMock() as self.ctx._GetTrackerFilenames:
      self.ctx._RetryFilter(e)
      self.assertFalse(self.ctx._GetTrackerFilenames.called)

  def testNoRemoveTrackerFileOnOtherErrors(self):
    """Test that we do not attempt to delete tracker files for other errors."""
    cmd = ['gsutil', 'cp', self.REMOTE_PATH, self.LOCAL_PATH]
    e = self._getException(cmd, 'InvalidUriError')

    with mock.MagicMock() as self.ctx._GetTrackerFilenames:
      self.ctx._RetryFilter(e)
      self.assertFalse(self.ctx._GetTrackerFilenames.called)


class GSContextTest(AbstractGSContextTest):
  """Tests for GSContext()"""

  def testSetAclError(self):
    """Ensure SetACL blows up if the acl isn't specified."""
    self.assertRaises(gs.GSContextException, self.ctx.SetACL, 'gs://abc/3')

  def testSetDefaultAcl(self):
    """Test default ACL behavior."""
    self.ctx.SetACL('gs://abc/1', 'monkeys')
    self.gs_mock.assertCommandContains(['acl', 'set', 'monkeys', 'gs://abc/1'])

  def testSetAcl(self):
    """Base ACL setting functionality."""
    ctx = gs.GSContext(acl='/my/file/acl')
    ctx.SetACL('gs://abc/1')
    self.gs_mock.assertCommandContains(['acl', 'set', '/my/file/acl',
                                        'gs://abc/1'])

  def testIncrement(self):
    """Test ability to atomically increment a counter."""
    ctx = gs.GSContext()
    ctx.Counter('gs://abc/1').Increment()
    self.gs_mock.assertCommandContains(['cp', '-', 'gs://abc/1'])

  def testGetGeneration(self):
    """Test ability to get the generation of a file."""
    ctx = gs.GSContext()
    ctx.GetGeneration('gs://abc/1')
    self.gs_mock.assertCommandContains(['acl', 'get', 'gs://abc/1'])

  def testCreateCached(self):
    """Test that the function runs through."""
    gs.GSContext(cache_dir=self.tempdir)

  def testReuseCached(self):
    """Test that second fetch is a cache hit."""
    gs.GSContext(cache_dir=self.tempdir)
    gs.GSUTIL_URL = None
    gs.GSContext(cache_dir=self.tempdir)

  def testWaitForGsPathsAllPresent(self):
    ctx = gs.GSContext()
    ctx.WaitForGsPaths(['/path1', '/path2'], 20)

  # TODO(dgarrett): We should add a test that first fails then succeeds finding
  # GS files, but I can't figure out how to make the Mock do that.

  def testWaitForGsPathsTimeout(self):
    self.gs_mock.AddCmdResult(['stat', '/path1'],
                              returncode=1,
                              error='GSResponseError code=NoSuchKey')
    ctx = gs.GSContext()
    self.assertRaises(gs.timeout_util.TimeoutError,
                      ctx.WaitForGsPaths, ['/path1', '/path2'],
                      timeout=1, period=0.02)

  def testNoParallelOpWithStdin(self):
    """Tests that "-m" is not used when we pipe the input."""
    ctx = gs.GSContext()
    ctx.Copy('-', 'gs://abc/1', input='foo')
    self.gs_mock.assertCommandContains(['-m'], expected=False)


class NetworkGSContextTest(cros_test_lib.TempDirTestCase):
  """Tests for GSContext that go over the network."""

  @cros_test_lib.NetworkTest()
  def testIncrement(self):
    ctx = gs.GSContext()
    with gs.TemporaryURL('testIncrement') as url:
      counter = ctx.Counter(url)
      self.assertEqual(0, counter.Get())
      for i in xrange(1, 4):
        self.assertEqual(i, counter.Increment())
        self.assertEqual(i, counter.Get())


class InitBotoTest(AbstractGSContextTest):
  """Test boto file interactive initialization."""

  GS_LS_ERROR = """\
You are attempting to access protected data with no configured credentials.
Please see http://code.google.com/apis/storage/docs/signup.html for
details about activating the Google Cloud Storage service and then run the
"gsutil config" command to configure gsutil to use these credentials."""

  GS_LS_ERROR2 = """\
GSResponseError: status=400, code=MissingSecurityHeader, reason=Bad Request, \
detail=Authorization."""

  GS_LS_BENIGN = """\
"GSResponseError: status=400, code=MissingSecurityHeader, reason=Bad Request,
detail=A nonempty x-goog-project-id header is required for this request."""

  def setUp(self):
    self.boto_file = os.path.join(self.tempdir, 'boto_file')
    self.ctx = gs.GSContext(boto_file=self.boto_file)

  def testGSLsSkippableError(self):
    """Benign GS error."""
    self.gs_mock.AddCmdResult(['ls'], returncode=1, error=self.GS_LS_BENIGN)
    self.assertTrue(self.ctx._TestGSLs())

  def testGSLsAuthorizationError1(self):
    """GS authorization error 1."""
    self.gs_mock.AddCmdResult(['ls'], returncode=1, error=self.GS_LS_ERROR)
    self.assertFalse(self.ctx._TestGSLs())

  def testGSLsError2(self):
    """GS authorization error 2."""
    self.gs_mock.AddCmdResult(['ls'], returncode=1, error=self.GS_LS_ERROR2)
    self.assertFalse(self.ctx._TestGSLs())

  def _WriteBotoFile(self, contents, *_args, **_kwargs):
    osutils.WriteFile(self.ctx.boto_file, contents)

  def testInitGSLsFailButSuccess(self):
    """Invalid GS Config, but we config properly."""
    self.gs_mock.AddCmdResult(['ls'], returncode=1, error=self.GS_LS_ERROR)
    self.ctx._InitBoto()

  def _AddLsConfigResult(self, side_effect=None):
    self.gs_mock.AddCmdResult(['ls'], returncode=1, error=self.GS_LS_ERROR)
    self.gs_mock.AddCmdResult(['config'], returncode=1, side_effect=side_effect)

  def testGSLsFailAndConfigError(self):
    """Invalid GS Config, and we fail to config."""
    self._AddLsConfigResult(
        side_effect=functools.partial(self._WriteBotoFile, 'monkeys'))
    self.assertRaises(cros_build_lib.RunCommandError, self.ctx._InitBoto)

  def testGSLsFailAndEmptyConfigFile(self):
    """Invalid GS Config, and we raise error on empty config file."""
    self._AddLsConfigResult(
        side_effect=functools.partial(self._WriteBotoFile, ''))
    self.assertRaises(gs.GSContextException, self.ctx._InitBoto)


if __name__ == '__main__':
  cros_test_lib.main()
