#!/usr/bin/python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the gs.py module."""

import functools
import mock
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


def PatchGS(*args, **kwargs):
  """Convenience method for patching GSContext."""
  return mock.patch.object(gs.GSContext, *args, **kwargs)


class GSContextMock(partial_mock.PartialCmdMock):
  """Used to mock out the GSContext class."""
  TARGET = 'chromite.lib.gs.GSContext'
  ATTRS = ('_DoCommand', '__init__', 'DEFAULT_SLEEP_TIME', 'DEFAULT_RETRIES')
  DEFAULT_ATTR = '_DoCommand'

  GSResponsePreconditionFailed = """
[Setting Content-Type=text/x-python]
        GSResponseError:: status=412, code=PreconditionFailed,
        reason=Precondition Failed."""

  DEFAULT_SLEEP_TIME = 0
  DEFAULT_RETRIES = 2

  def PreStart(self):
    os.environ.pop("BOTO_CONFIG", None)

  def _target__init__(self, *args, **kwargs):
    with PatchGS('_CheckFile', return_value=True):
      self.backup['__init__'](*args, **kwargs)

  def _DoCommand(self, inst, gsutil_cmd, **kwargs):
    result = self._results['_DoCommand'].LookupResult(
        (gsutil_cmd,), hook_args=(inst, gsutil_cmd,), hook_kwargs=kwargs)

    rc_mock = cros_build_lib_unittest.RunCommandMock()
    rc_mock.AddCmdResult(
        partial_mock.ListRegex('gsutil'), result.returncode, result.output,
        result.error)

    with rc_mock:
      return self.backup['_DoCommand'](inst, gsutil_cmd, **kwargs)


class AbstractGSContextTest(cros_test_lib.MockTempDirTestCase):
  """Base class for GSContext tests."""

  def setUp(self):
    self.gs_mock = GSContextMock()
    self.StartPatcher(self.gs_mock)
    self.gs_mock.SetDefaultCmdResult()
    self.ctx = gs.GSContext()


class CopyTest(AbstractGSContextTest):
  """Tests GSContext.Copy() functionality."""

  LOCAL_PATH = '/tmp/file'
  GIVEN_REMOTE = EXPECTED_REMOTE = 'gs://test/path/file'
  ACL_FILE = '/my/file/acl'
  ACL_FILE2 = '/my/file/other'

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

  def testWithACLFile(self):
    """ACL specified during init."""
    ctx = gs.GSContext(acl_file=self.ACL_FILE)
    self.Copy(ctx=ctx)
    self.gs_mock.assertCommandContains(['cp', '-a', self.ACL_FILE])

  def testWithACLFile2(self):
    """ACL specified during invocation."""
    self.Copy(acl=self.ACL_FILE)
    self.gs_mock.assertCommandContains(['cp', '-a', self.ACL_FILE])

  def testWithACLFile3(self):
    """ACL specified during invocation that overrides init."""
    ctx = gs.GSContext(acl_file=self.ACL_FILE)
    self.Copy(ctx=ctx, acl=self.ACL_FILE2)
    self.gs_mock.assertCommandContains(['cp', '-a', self.ACL_FILE2])

  def testVersion(self):
    """Test version field."""
    for version in xrange(7):
      self.Copy(version=version)
      self.gs_mock.assertCommandContains(
          [], headers=['x-goog-if-sequence-number-match:%s' % version])

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
    with PatchGS('DEFAULT_GSUTIL_BIN', new=self.bad_path):
      self.assertRaises(gs.GSContextException, gs.GSContext)

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
    with PatchGS('DEFAULT_GSUTIL_BIN', self.bad_path):
      self.assertRaises(gs.GSContextException, gs.GSContext)

  def testInitAclFile(self):
    """Test ACL selection logic in __init__."""
    self.assertEqual(gs.GSContext().acl_file, None)
    self.assertEqual(gs.GSContext(acl_file=self.acl_file).acl_file,
                     self.acl_file)
    self.assertRaises(gs.GSContextException, gs.GSContext,
                      acl_file=self.bad_path)


class GSContextTest(AbstractGSContextTest):
  """Tests for GSContext()"""

  def _testDoCommand(self, ctx, retries, sleep):
    with mock.patch.object(cros_build_lib, 'RetryCommand', autospec=True):
      ctx.Copy('/blah', 'gs://foon')
      cmd = [self.ctx.gsutil_bin, 'cp', '--', '/blah', 'gs://foon']
      cros_build_lib.RetryCommand.assert_called_once_with(
          mock.ANY, retries, cmd, sleep=sleep, redirect_stderr=True,
          extra_env={'BOTO_CONFIG': mock.ANY})

  def testDoCommandDefault(self):
    """Verify the internal DoCommand function works correctly."""
    self._testDoCommand(self.ctx, retries=self.ctx.DEFAULT_RETRIES,
                        sleep=self.ctx.DEFAULT_SLEEP_TIME)

  def testDoCommandCustom(self):
    """Test that retries and sleep parameters are honored."""
    ctx = gs.GSContext(retries=4, sleep=1)
    self._testDoCommand(ctx, retries=4, sleep=1)

  def testSetAclError(self):
    """Ensure SetACL blows up if the acl isn't specified."""
    self.assertRaises(gs.GSContextException, self.ctx.SetACL, 'gs://abc/3')

  def testSetDefaultAcl(self):
    """Test default ACL behavior."""
    self.ctx.SetACL('gs://abc/1', 'monkeys')
    self.gs_mock.assertCommandContains(['setacl', 'monkeys', 'gs://abc/1'])

  def testSetAcl(self):
    """Base ACL setting functionality."""
    ctx = gs.GSContext(acl_file='/my/file/acl')
    ctx.SetACL('gs://abc/1')
    self.gs_mock.assertCommandContains(['setacl', '/my/file/acl',
                                        'gs://abc/1'])


class InitBotoTest(AbstractGSContextTest):
  """Test boto file interactive initialization."""

  GS_LS_ERROR = """\
You are attempting to access protected data with no configured credentials.
Please see http://code.google.com/apis/storage/docs/signup.html for
details about activating the Google Cloud Storage service and then run the
"gsutil config" command to configure gsutil to use these credentials."""

  GS_LS_BENIGN = """\
"GSResponseError: status=400, code=MissingSecurityHeader, reason=Bad Request,
detail=A nonempty x-goog-project-id header is required for this request."""

  def testInitGSLsSkippableError(self):
    """Benign GS error."""
    self.gs_mock.AddCmdResult(['ls'], returncode=1, error=self.GS_LS_BENIGN)
    self.ctx._InitBoto()

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
