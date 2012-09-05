#!/usr/bin/python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the gs.py module."""

import os
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

import mox
import unittest

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import osutils


#pylint: disable=E1101,W0212
class GSContextTest(cros_test_lib.TempDirMixin, mox.MoxTestBase):
  """Tests for GSContext()"""

  _GSResponsePreconditionFailed = """
[Setting Content-Type=text/x-python]
        GSResponseError:: status=412, code=PreconditionFailed,
        reason=Precondition Failed."""

  def setUp(self):
    cros_test_lib.TempDirMixin.setUp(self)
    for attr in ('gsutil_bin', 'boto_file', 'acl_file'):
      path = os.path.join(self.tempdir, attr)
      osutils.WriteFile(path, '')
      setattr(self, attr, path)
    self.defaults = {'DEFAULT_BOTO_FILE': self.boto_file,
                     'DEFAULT_GSUTIL_BIN': self.gsutil_bin}
    self.bad_path = os.path.join(self.tempdir, 'nonexistent')

    self._boto_config_env = os.environ.pop("BOTO_CONFIG", None)
    mox.MoxTestBase.setUp(self)

    # No command should be ran w/out us explicitly asseting it.
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')

  def tearDown(self):
    mox.MoxTestBase.tearDown(self)
    cros_test_lib.TempDirMixin.tearDown(self)
    if self._boto_config_env is not None:
      os.environ['BOTO_CONFIG'] = self._boto_config_env

  def MkContext(self, *args, **kwds):
    # Note we derive on the fly here so we can ensure that
    # there always is a valid fallback for boto_file; we
    # do so in a way that allows us to also check that
    # behaviour when the default is missing.

    class Context(gs.GSContext):
      # This bit of voodoo is basically a way to scrape
      # default overrides out of keywords into the class
      # scope; used for testing the defaults.
      # First inject in our defaults, then the overrides.
      locals().update(self.defaults)
      locals().update((key, kwds.pop(key)) for key in list(kwds)
                      if key.startswith("DEFAULT_"))

    suppress_DoCommand = kwds.pop('suppress_DoCommand', True)
    context = Context(*args, **kwds)
    if suppress_DoCommand:
      self.mox.StubOutWithMock(context, '_DoCommand')
    return context

  def testInitGsutilBin(self):
    """Test we use the given gsutil binary, erroring where appropriate."""

    self.assertEqual(self.MkContext().gsutil_bin, self.gsutil_bin)
    self.assertRaises(gs.GSContextException, self.MkContext,
                      DEFAULT_BOTO_FILE=self.bad_path)
    self.assertEqual(
        self.MkContext(gsutil_bin=self.boto_file).gsutil_bin,
        self.boto_file)
    self.assertRaises(gs.GSContextException, self.MkContext,
                      gsutil_bin=self.bad_path)

  def testInitBotoFile(self):
    """Test we use the given boto_file, erroring where appropriate."""

    self.assertEqual(self.MkContext().boto_file, self.boto_file)
    self.assertRaises(gs.GSContextException, self.MkContext,
                      DEFAULT_BOTO_FILE=self.bad_path)
    # Check env usage next; no need to cleanup, teardown handles it,
    # and we want the env var to persist for the next part of this test.
    os.environ['BOTO_CONFIG'] = self.bad_path
    self.assertRaises(gs.GSContextException, self.MkContext)
    os.environ['BOTO_CONFIG'] = self.gsutil_bin
    self.assertEqual(self.MkContext().boto_file, self.gsutil_bin)

    self.assertEqual(self.MkContext(boto_file=self.acl_file).boto_file,
                     self.acl_file)
    self.assertRaises(gs.GSContextException, self.MkContext,
                      boto_file=self.bad_path)

  def testInitAclFile(self):
    self.assertEqual(self.MkContext().acl_file, None)
    self.assertEqual(self.MkContext(acl_file=self.acl_file).acl_file,
                     self.acl_file)
    self.assertRaises(gs.GSContextException, self.MkContext,
                      acl_file=self.bad_path)

  def assertGSCommand(self, context, cmd, **kwds):
    return context._DoCommand(cmd, **kwds)

  def testCopy(self, functor=None, filename=None):
    """Test CopyTo functionality."""

    local_path = lambda  x: '/tmp/file%s' % x
    if filename is not None:
      given_remote = lambda x: 'gs://test/path/%s' % x
      expected_remote = lambda x: 'gs://test/path/%s/%s' % (x, filename)
    else:
      given_remote = expected_remote = lambda x: 'gs://test/path/file%s' % x

    ctx1 = self.MkContext()
    self.assertGSCommand(ctx1, ['cp', '--', local_path(1), expected_remote(1)])
    ctx2 = self.MkContext(acl_file=self.acl_file)
    self.assertGSCommand(ctx2, ['cp', '-a', self.acl_file, '--',
                                local_path(2), expected_remote(2)])
    ctx3 = self.MkContext()
    self.assertGSCommand(ctx3, ['cp', '-a', self.gsutil_bin, '--',
                                local_path('brass'),
                                expected_remote('brass')])
    ctx4 = self.MkContext(acl_file=self.gsutil_bin)
    self.assertGSCommand(ctx4, ['cp', '-a', self.acl_file, '--',
                                local_path('monkey'),
                                expected_remote('monkey')])
    ctx5 = self.MkContext()
    self.assertGSCommand(ctx5, ['cp', '--', local_path(5),
                                expected_remote(5)],
                         headers=['x-goog-if-sequence-number-match:0'])
    ctx6 = self.MkContext()
    self.assertGSCommand(ctx6, ['cp', '--', self.gsutil_bin,
                                expected_remote(6)],
                         headers=['x-goog-if-sequence-number-match:6'])
    ctx7 = self.MkContext()
    ret = self.assertGSCommand(ctx7, ['cp', '--', local_path(7),
                                      expected_remote(7)])
    ret.AndRaise(cros_build_lib.RunCommandError(
        'blah', cros_build_lib.CommandResult(returncode=1, output='')))
    ctx8 = self.MkContext()
    ret = self.assertGSCommand(ctx8, ['cp', '--', local_path(8),
                                      expected_remote(8)])
    ret.AndRaise(cros_build_lib.RunCommandError(
        'blah', cros_build_lib.CommandResult(returncode=1,
                                             output='code=PreconditionFailed')))

    self.mox.ReplayAll()
    if functor is None:
      functor = lambda ctx:ctx.Copy
    functor(ctx1)(local_path(1), given_remote(1))
    functor(ctx2)(local_path(2), given_remote(2))
    functor(ctx3)(local_path('brass'), given_remote('brass'),
                  acl=self.gsutil_bin)
    functor(ctx4)(local_path('monkey'), given_remote('monkey'),
                  acl=self.acl_file)
    functor(ctx5)(local_path(5), given_remote(5), version=0)
    functor(ctx6)(self.gsutil_bin, given_remote(6), version=6)
    self.assertRaises(cros_build_lib.RunCommandError,
                      functor(ctx7), local_path(7), given_remote(7))
    self.assertRaises(gs.GSContextException,
                      functor(ctx8), local_path(8), given_remote(8))
    self.mox.VerifyAll()

  def testCopyInto(self):
    def functor(ctx):
      return lambda *args, **kwds:ctx.CopyInto(*args, filename='blah', **kwds)
    self.testCopy(functor=functor, filename='blah')

  def testDoCommand(self):
    """Verify the internal DoCommand function works correctly."""
    self.mox.StubOutWithMock(cros_build_lib, 'RetryCommand')
    ctx1 = self.MkContext(suppress_DoCommand=False)
    cros_build_lib.RetryCommand(
       cros_build_lib.RunCommandCaptureOutput, ctx1._retries,
       [self.gsutil_bin, 'cp', '--', '/blah', 'gs://foon'],
       sleep=ctx1._sleep_time, extra_env={'BOTO_CONFIG':self.boto_file})
    ctx2 = self.MkContext(suppress_DoCommand=False, retries=2, sleep=1)
    cros_build_lib.RetryCommand(
       cros_build_lib.RunCommandCaptureOutput, ctx2._retries,
       [self.gsutil_bin, '-h',  'x-goog-if-sequence-number-match:1',
        'cp', '--', '/blah', 'gs://foon'],
       sleep=ctx2._sleep_time, extra_env={'BOTO_CONFIG':self.boto_file})

    self.mox.ReplayAll()
    ctx1.Copy('/blah', 'gs://foon')
    ctx2.Copy('/blah', 'gs://foon', version=1)
    self.mox.VerifyAll()

  def testSetAcl(self):
    ctx1 = self.MkContext()
    self.assertGSCommand(ctx1, ['setacl', 'monkeys', 'gs://abc/1'])
    ctx2 = self.MkContext(acl_file=self.acl_file)
    self.assertGSCommand(ctx2, ['setacl', self.acl_file, 'gs://abc/2'])
    # Ensure it blows up if there isn't an acl specified in any fashion.
    ctx3 = self.MkContext()

    self.mox.ReplayAll()
    ctx1.SetACL('gs://abc/1', 'monkeys')
    ctx2.SetACL('gs://abc/2')
    self.assertRaises(gs.GSContextException, ctx3.SetACL, 'gs://abc/3')
    self.mox.VerifyAll()

if __name__ == '__main__':
  cros_build_lib.SetupBasicLogging()
  unittest.main()


