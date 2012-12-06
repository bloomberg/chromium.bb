#!/usr/bin/python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the gs.py module."""

import mock
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import partial_mock


class GSContextMock(partial_mock.PartialCmdMock):
  """Used to mock out the GSContext class."""
  TARGET = 'chromite.lib.gs.GSContext'
  ATTRS = ('_DoCommand', 'DEFAULT_BOTO_FILE', 'DEFAULT_GSUTIL_BIN',
           'DEFAULT_SLEEP_TIME', 'DEFAULT_RETRIES')
  DEFAULT_ATTR = '_DoCommand'

  GSResponsePreconditionFailed = """
[Setting Content-Type=text/x-python]
        GSResponseError:: status=412, code=PreconditionFailed,
        reason=Precondition Failed."""

  DEFAULT_SLEEP_TIME = 0
  DEFAULT_RETRIES = 2

  def __init__(self, **kwargs):
    partial_mock.PartialCmdMock.__init__(self, create_tempdir=True)
    self.overrides = [(key, kwargs.pop(key)) for key in list(kwargs)
                      if key.startswith("DEFAULT_")]
    self.boto_file = None
    self.gsutil_bin = None
    self.acl_file = None

  def PreStart(self):
    # Note we derive on the fly here so we can ensure that
    # there always is a valid fallback for boto_file; we
    # do so in a way that allows us to also check that
    # behaviour when the default is missing.
    file_list = ['gsutil_bin', 'boto_file', 'acl_file']
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, file_list)

    for f in file_list:
      setattr(self, f, os.path.join(self.tempdir, f))

    defaults = {
        'DEFAULT_BOTO_FILE': self.boto_file,
        'DEFAULT_GSUTIL_BIN': self.gsutil_bin}
    defaults.update(self.overrides)
    # Protect ourselves from preexisting BOTO_CONFIG env settings.
    # Environment variables are restored during Stop().
    os.environ.pop("BOTO_CONFIG", None)
    self.__dict__.update(defaults)

  def _DoCommand(self, inst, gsutil_cmd, **kwargs):
    result = self._results['_DoCommand'].LookupResult(
        (gsutil_cmd,), hook_args=(inst, gsutil_cmd,), hook_kwargs=kwargs)

    rc_mock = cros_build_lib_unittest.RunCommandMock()
    rc_mock.AddCmdResult(
        partial_mock.ListRegex('gsutil'), result.returncode, result.output,
        result.error)

    with rc_mock:
      return self.backup['_DoCommand'](inst, gsutil_cmd, **kwargs)


class AbstractGSContextTest(cros_test_lib.TempDirTestCase):
  """Base class for GSContext tests."""

  def setUp(self):
    self.gs_mock = GSContextMock().Start()
    self.gs_mock.SetDefaultCmdResult()
    self.ctx = gs.GSContext()

  def tearDown(self):
    if self.gs_mock:
      self.gs_mock.Stop()


class CopyTest(AbstractGSContextTest):
  """Tests GSContext.Copy() functionality."""

  LOCAL_PATH = '/tmp/file'
  GIVEN_REMOTE = EXPECTED_REMOTE = 'gs://test/path/file'

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
    ctx = gs.GSContext(acl_file=self.gs_mock.acl_file)
    self.Copy(ctx=ctx)
    self.gs_mock.assertCommandContains(['cp', '-a', self.gs_mock.acl_file])

  def testWithACLFile2(self):
    """ACL specified during invocation."""
    self.Copy(acl=self.gs_mock.gsutil_bin)
    self.gs_mock.assertCommandContains(['cp', '-a', self.gs_mock.gsutil_bin])

  def testWithACLFile3(self):
    """ACL specified during invocation that overrides init."""
    ctx = gs.GSContext(acl_file=self.gs_mock.gsutil_bin)
    self.Copy(ctx=ctx, acl=self.gs_mock.acl_file)
    self.gs_mock.assertCommandContains(['cp', '-a', self.gs_mock.acl_file])

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
        output=self.gs_mock.GSResponsePreconditionFailed)
    self.assertRaises(gs.GSContextException, self.Copy)


class CopyIntoTest(CopyTest):
  """Test CopyInto functionality."""

  FILE = 'ooga'
  GIVEN_REMOTE = 'gs://test/path/file'
  EXPECTED_REMOTE = '%s/%s' % (GIVEN_REMOTE, FILE)

  def _Copy(self, ctx, *args, **kwargs):
    return ctx.CopyInto(*args, filename=self.FILE, **kwargs)


#pylint: disable=E1101,W0212
class GSContextTest(AbstractGSContextTest):
  """Tests for GSContext()"""
  def setUp(self):
    self.bad_path = os.path.join(self.tempdir, 'nonexistent')

  def testInitGsutilBin(self):
    """Test we use the given gsutil binary, erroring where appropriate."""
    self.assertEquals(gs.GSContext().gsutil_bin, self.gs_mock.gsutil_bin)
    self.assertRaises(gs.GSContextException, gs.GSContext,
                      gsutil_bin=self.bad_path)

  def testBadGSUtilBin(self):
    """Test exception thrown for bad gsutil paths."""
    self.gs_mock.Stop()
    with GSContextMock(DEFAULT_GSUTIL_BIN=self.bad_path):
      self.assertRaises(gs.GSContextException, gs.GSContext)

  def testInitBotoFileEnv(self):
    os.environ['BOTO_CONFIG'] = self.gs_mock.gsutil_bin
    self.assertTrue(gs.GSContext().boto_file, self.gs_mock.gsutil_bin)
    self.assertEqual(gs.GSContext(boto_file=self.gs_mock.acl_file).boto_file,
                     self.gs_mock.acl_file)
    self.assertRaises(gs.GSContextException, gs.GSContext,
                      boto_file=self.bad_path)

  def testInitBotoFileEnvError(self):
    """Boto file through env var error."""
    self.assertEquals(gs.GSContext().boto_file, self.gs_mock.boto_file)
    # Check env usage next; no need to cleanup, teardown handles it,
    # and we want the env var to persist for the next part of this test.
    os.environ['BOTO_CONFIG'] = self.bad_path
    self.assertRaises(gs.GSContextException, gs.GSContext)

  def testInitBotoFileError(self):
    """Test bad boto file."""
    self.gs_mock.Stop()
    with GSContextMock(DEFAULT_BOTO_FILE=self.bad_path):
      self.assertRaises(gs.GSContextException, gs.GSContext)

  def testInitAclFile(self):
    """Test ACL selection logic in __init__."""
    self.assertEqual(gs.GSContext().acl_file, None)
    self.assertEqual(gs.GSContext(acl_file=self.gs_mock.acl_file).acl_file,
                     self.gs_mock.acl_file)
    self.assertRaises(gs.GSContextException, gs.GSContext,
                      acl_file=self.bad_path)

  def _testDoCommand(self, ctx, retries, sleep):
    with mock.patch.object(cros_build_lib, 'RetryCommand', autospec=True):
      ctx.Copy('/blah', 'gs://foon')
      cmd = [self.ctx.gsutil_bin, 'cp', '--', '/blah', 'gs://foon']
      cros_build_lib.RetryCommand.assert_called_once_with(
          mock.ANY, retries, cmd, sleep=sleep,
          extra_env={'BOTO_CONFIG': self.gs_mock.boto_file})

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
    ctx = gs.GSContext(acl_file=self.gs_mock.acl_file)
    ctx.SetACL('gs://abc/1')
    self.gs_mock.assertCommandContains(['setacl', self.gs_mock.acl_file,
                                        'gs://abc/1'])


if __name__ == '__main__':
  cros_test_lib.main()
