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

import logging
import mox
import tempfile
import unittest

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gs

class GSContextTest(cros_test_lib.TempDirMixin, mox.MoxTestBase):
  """Tests for GSContext()"""

  _GSResponsePreconditionFailed = """
[Setting Content-Type=text/x-python]
        GSResponseError:: status=412, code=PreconditionFailed,
        reason=Precondition Failed."""

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    cros_test_lib.TempDirMixin.setUp(self)

    fd0, self.gsutil_test = tempfile.mkstemp()
    fd1, self.boto_test = tempfile.mkstemp()
    fd2, self.acl_test = tempfile.mkstemp()

    os.close(fd0)
    os.close(fd1)
    os.close(fd2)

    self.boto_env = {'BOTO_CONFIG': self.boto_test}

  def tearDown(self):
    cros_test_lib.TempDirMixin.tearDown(self)

  def _contextSetup(self):
    """Common mock out constructor."""
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    cmd = [self.gsutil_test, 'version']
    cros_build_lib.RunCommand(cmd, debug_level=logging.DEBUG,
                              extra_env=self.boto_env,
                              log_output=True)

  def _Run(self, cmd):
    """Common RunCommand invocation."""
    return cros_build_lib.RunCommandWithRetries(gs.GSContext._RETRIES,
                                                cmd,
                                                sleep=gs.GSContext._SLEEP_TIME,
                                                debug_level=logging.DEBUG,
                                                extra_env=self.boto_env,
                                                log_output=True,
                                                combine_stdout_stderr=True)

  def _RunACL(self, cmd):
    """RunCommand invocation for SetACL method."""
    return cros_build_lib.RunCommandWithRetries(gs.GSContext._RETRIES,
                                                cmd,
                                                sleep=gs.GSContext._SLEEP_TIME,
                                                debug_level=logging.DEBUG,
                                                extra_env=self.boto_env)

  def testContextGSBinExists(self):
    """Test we use the given gsutil binary."""
    self._contextSetup()
    self.mox.ReplayAll()
    my_gs = gs.GSContext(self.gsutil_test, boto_file=self.boto_test)
    self.mox.VerifyAll()

    self.assertEqual(self.gsutil_test, my_gs.gsutil_bin)

  def testContextGSBinNotExists(self):
    """Test if the path to the gsutil binary does not exist."""
    self.assertRaises(gs.GSContextException, gs.GSContext, '/does/not/exist')

  def testContextBotoExists(self):
    """Test a given boto file is used."""
    self._contextSetup()
    self.mox.ReplayAll()
    gs.GSContext(self.gsutil_test, boto_file=self.boto_test)
    self.mox.VerifyAll()

  def testContextBotoNotExists(self):
    """Test a given boto file exists."""
    self.assertRaises(gs.GSContextException, gs.GSContext, self.gsutil_test,
                      '/boto/does/not/exist')

  def testContextAclExists(self):
    """Test a given acl file is set."""
    self._contextSetup()
    self.mox.ReplayAll()
    my_gs = gs.GSContext(self.gsutil_test, boto_file=self.boto_test,
                        acl_file=self.acl_test)
    self.mox.VerifyAll()

    self.assertEqual(self.acl_test, my_gs.acl_file)

  def testContextAclNotExists(self):
    """Test a given acl file exists."""
    self.assertRaises(gs.GSContextException, gs.GSContext, self.gsutil_test,
                      self.boto_test, '/acl/does/not/exist')

  def test_UploadNoAcl(self):
    """Test _Upload with no ACL."""
    self._contextSetup()
    local_path = '/tmp/file1.txt'
    remote_file = 'gs://test/path/file1.txt'

    cmd = [self.gsutil_test, 'cp', local_path, remote_file]
    self._Run(cmd)

    self.mox.ReplayAll()
    my_gs = gs.GSContext(self.gsutil_test, boto_file=self.boto_test)
    my_gs._Upload(local_path, remote_file)
    self.mox.VerifyAll()

  def test_UploadWithAcl(self):
    """Test _Upload with ACL."""
    self._contextSetup()
    local_path = '/tmp/file1.txt'
    remote_file = 'gs://test/path/file1.txt'

    cmd = [self.gsutil_test, 'cp', '-a', 'private', local_path, remote_file]
    self._Run(cmd)

    self.mox.ReplayAll()
    my_gs = gs.GSContext(self.gsutil_test, boto_file=self.boto_test)
    my_gs._Upload(local_path, remote_file, 'private')
    self.mox.VerifyAll()

  def test_UploadRaisesPreconditionFailed(self):
    """Test _Upload with a failed precondition."""
    self._contextSetup()
    local_path = '/tmp/file1.txt'
    remote_file = 'gs://test/path/file1.txt'

    result = cros_build_lib.CommandResult()
    result.output = self._GSResponsePreconditionFailed
    error = cros_build_lib.RunCommandError('Your command is not working.',
                                           result)

    cmd = [self.gsutil_test, 'cp', local_path, remote_file]
    ret = self._Run(cmd)
    ret.AndRaise(error)

    self.mox.ReplayAll()
    my_gs = gs.GSContext(self.gsutil_test, boto_file=self.boto_test)
    self.assertRaises(gs.GSContextPreconditionFailed,
                      my_gs._Upload, local_path, remote_file)
    self.mox.VerifyAll()

  def test_UploadRaisesRunCommandError(self):
    """Test _Upload is capable of throwing an error."""
    self._contextSetup()
    local_path = '/tmp/file1.txt'
    remote_file = 'gs://test/path/file1.txt'

    result = cros_build_lib.CommandResult()
    result.output = 'Not a precondition failed error.'
    error = cros_build_lib.RunCommandError('Your command is not working.',
                                           result)

    cmd = [self.gsutil_test, 'cp', local_path, remote_file]
    ret = self._Run(cmd)
    ret.AndRaise(error)

    self.mox.ReplayAll()
    my_gs = gs.GSContext(self.gsutil_test, boto_file=self.boto_test)
    self.assertRaises(cros_build_lib.RunCommandError,
                      my_gs._Upload, local_path, remote_file)
    self.mox.VerifyAll()


  def testUpload(self):
    """Test Upload works correctly."""
    self._contextSetup()
    local_path = '/tmp/file1.txt'
    upload_url = 'gs://test/path/'
    filename = 'file1.txt'

    cmd = [self.gsutil_test, 'cp', os.path.join(local_path, filename),
           os.path.join(upload_url, filename)]
    self._Run(cmd)

    self.mox.ReplayAll()
    my_gs = gs.GSContext(self.gsutil_test, boto_file=self.boto_test)
    my_gs.Upload(local_path, upload_url, filename)
    self.mox.VerifyAll()

  def testUploadACL(self):
    """Test Upload with ACL works correctly."""
    self._contextSetup()
    local_path = '/tmp/file1.txt'
    upload_url = 'gs://test/path/'
    filename = 'file1.txt'
    acl = 'public-read'

    cmd = [self.gsutil_test, 'cp', '-a', acl,
           os.path.join(local_path, filename),
           os.path.join(upload_url, filename)]
    self._Run(cmd)

    self.mox.ReplayAll()
    my_gs = gs.GSContext(self.gsutil_test, boto_file=self.boto_test)
    my_gs.Upload(local_path, upload_url, filename, acl)
    self.mox.VerifyAll()

  def testSetACLInit(self):
    """Test SetACL works when ACL set via constructor."""
    self._contextSetup()
    upload_url = 'gs://test/path/file1.txt'
    acl = self.acl_test

    cmd = [self.gsutil_test, 'setacl', acl, upload_url]
    self._RunACL(cmd)

    self.mox.ReplayAll()
    my_gs = gs.GSContext(self.gsutil_test, boto_file=self.boto_test,
                         acl_file=self.acl_test)
    # Should not have to specify acl here if we gave it to the constructor.
    my_gs.SetACL(upload_url)
    self.mox.VerifyAll()

  def testSetACLWithFile(self):
    """Test SetACL works given a permissions file."""
    self._contextSetup()
    upload_url = 'gs://test/path/file1.txt'
    acl = '/my/perms/acl.txt'

    cmd = [self.gsutil_test, 'setacl', acl, upload_url]
    self._RunACL(cmd)

    self.mox.ReplayAll()
    my_gs = gs.GSContext(self.gsutil_test, boto_file=self.boto_test)
    my_gs.SetACL(upload_url, acl)
    self.mox.VerifyAll()

  def testSetACLCanned(self):
    """Test SetACL works with a canned ACL."""
    self._contextSetup()
    upload_url = 'gs://test/path/file1.txt'
    acl = 'public-read'

    cmd = [self.gsutil_test, 'setacl', acl, upload_url]
    self._RunACL(cmd)

    self.mox.ReplayAll()
    my_gs = gs.GSContext(self.gsutil_test, boto_file=self.boto_test)
    my_gs.SetACL(upload_url, acl)
    self.mox.VerifyAll()

  def testUploadOnMatch(self):
    """Test UploadOnMatch with successful precondition match."""
    self._contextSetup()
    local_path = '/tmp/file1.txt'
    upload_url = 'gs://test/path/'
    filename = 'file1.txt'
    match = 0

    cmd = [self.gsutil_test, '-h', 'x-goog-if-sequence-number-match:%d'
           % match, 'cp', os.path.join(local_path, filename),
           os.path.join(upload_url, filename)]
    self._Run(cmd)

    self.mox.ReplayAll()
    my_gs = gs.GSContext(self.gsutil_test, boto_file=self.boto_test)
    my_gs.UploadOnMatch(local_path, upload_url, filename, version=match)
    self.mox.VerifyAll()

  def testUploadOnMatchFailed(self):
    """Test UploadOnMatch with a failed precondition match."""
    self._contextSetup()
    local_path = '/tmp/file1.txt'
    upload_url = 'gs://test/path/'
    filename = 'file1.txt'
    match = 0

    result = cros_build_lib.CommandResult()
    result.output = self._GSResponsePreconditionFailed
    error = cros_build_lib.RunCommandError('UploadOnMatch has failed.',
                                           result)

    cmd = [self.gsutil_test, '-h', 'x-goog-if-sequence-number-match:%d'
           % match, 'cp', os.path.join(local_path, filename),
           os.path.join(upload_url, filename)]
    ret = self._Run(cmd)
    ret.AndRaise(error)

    self.mox.ReplayAll()
    my_gs = gs.GSContext(self.gsutil_test, boto_file=self.boto_test)
    self.assertRaises(gs.GSContextPreconditionFailed, my_gs.UploadOnMatch,
                      local_path, upload_url, filename, version=match)
    self.mox.VerifyAll()

if __name__ == '__main__':
  cros_build_lib.SetupBasicLogging()
  unittest.main()


