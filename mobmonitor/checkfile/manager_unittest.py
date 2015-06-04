# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for Mob* Monitor checkfile manager."""

from __future__ import print_function

import mock
import imp
import os
import subprocess
import time
import threading

from cherrypy.process import plugins
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.mobmonitor.checkfile import manager

# Test health check and related attributes
class TestHealthCheck(object):
  """Test health check."""

  def Check(self):
    """Stub Check."""
    return 0

  def Diagnose(self, _errcode):
    """Stub Diagnose."""
    return ('Unknown Error.', [])

TEST_SERVICE_NAME = 'test-service'
TEST_MTIME = 100
CHECKDIR = '.'

# Strings that are used to mock actual check modules.
CHECKFILE_MANY_SIMPLE = '''
SERVICE = 'test-service'

class MyHealthCheck2(object):
  def Check(self):
    return 0

  def Diagnose(self, errcode):
    return ('Unknown error.', [])

class MyHealthCheck3(object):
  def Check(self):
    return 0

  def Diagnose(self, errcode):
    return ('Unknown error.', [])

class MyHealthCheck4(object):
  def Check(self):
    return 0

  def Diagnose(self, errcode):
    return ('Unknown error.', [])
'''

CHECKFILE_MANY_SIMPLE_ONE_BAD = '''
SERVICE = 'test-service'

class MyHealthCheck(object):
  def Check(self):
    return 0

  def Diagnose(self, errcode):
    return ('Unknown error.', [])

class NotAHealthCheck(object):
  def Diagnose(self, errcode):
    return ('Unknown error.', [])

class MyHealthCheck2(object):
  def Check(self):
    return 0

  def Diagnose(self, errcode):
    return ('Unknown error.', [])
'''

NOT_A_CHECKFILE = '''
class NotAHealthCheck(object):
  def NotCheckNorDiagnose(self):
    return -1
'''

ANOTHER_NOT_A_CHECKFILE = '''
class AnotherNotAHealthCheck(object):
  def AnotherNotCheckNorDiagnose(self):
    return -2
'''


class RunCommand(threading.Thread):
  """Helper class for executing the Mob* Monitor with a timeout."""

  def __init__(self, cmd, timeout):
    threading.Thread.__init__(self)
    self.cmd = cmd
    self.timeout = timeout
    self.p = None

  def run(self):
    self.p = subprocess.Popen(self.cmd, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT)
    self.p.wait()

  def Stop(self):
    self.join(self.timeout)

    if self.is_alive():
      self.p.terminate()
      self.join()

    return self.p.stdout.read()


class CheckFileManagerHelperTest(cros_test_lib.MockTestCase):
  """Unittests for CheckFileManager helper functions."""

  def testIsHealthCheck(self):
    """Test that IsHealthCheck properly asserts the health check interface."""

    class NoAttrs(object):
      """Test health check missing 'check' and 'diagnose' methods."""

    class NoCheckAttr(object):
      """Test health check missing 'check' method."""
      def Diagnose(self, errcode):
        pass

    class NoDiagnoseAttr(object):
      """Test health check missing 'diagnose' method."""
      def Check(self):
        pass

    class GoodHealthCheck(object):
      """Test health check that implements 'check' and 'diagnose' methods."""
      def Check(self):
        pass

      def Diagnose(self, errcode):
        pass

    self.assertFalse(manager.IsHealthCheck(NoAttrs()))
    self.assertFalse(manager.IsHealthCheck(NoCheckAttr()))
    self.assertFalse(manager.IsHealthCheck(NoDiagnoseAttr()))
    self.assertTrue(manager.IsHealthCheck(GoodHealthCheck()))

  def testImportCheckFileAllHealthChecks(self):
    """Test that health checks and service name are collected."""
    self.StartPatcher(mock.patch('os.path.splitext'))
    os.path.splitext.return_value = '/path/to/test_check.py'

    self.StartPatcher(mock.patch('os.path.getmtime'))
    os.path.getmtime.return_value = TEST_MTIME

    checkmodule = imp.new_module('test_check')
    exec CHECKFILE_MANY_SIMPLE in checkmodule.__dict__
    self.StartPatcher(mock.patch('imp.load_source'))
    imp.load_source.return_value = checkmodule

    service_name, healthchecks, mtime = manager.ImportCheckfile('/')

    self.assertEquals(service_name, 'test-service')
    self.assertEquals(len(healthchecks), 3)
    self.assertEquals(mtime, TEST_MTIME)

  def testImportCheckFileSomeHealthChecks(self):
    """Test importing when not all classes are actually health checks."""
    self.StartPatcher(mock.patch('os.path.splitext'))
    os.path.splitext.return_value = '/path/to/test_check.py'

    self.StartPatcher(mock.patch('os.path.getmtime'))
    os.path.getmtime.return_value = TEST_MTIME

    checkmodule = imp.new_module('test_check')
    exec CHECKFILE_MANY_SIMPLE_ONE_BAD in checkmodule.__dict__
    self.StartPatcher(mock.patch('imp.load_source'))
    imp.load_source.return_value = checkmodule

    service_name, healthchecks, mtime = manager.ImportCheckfile('/')

    self.assertEquals(service_name, 'test-service')
    self.assertEquals(len(healthchecks), 2)
    self.assertEquals(mtime, TEST_MTIME)


class CheckFileManagerTest(cros_test_lib.MockTestCase):
  """Unittests for CheckFileManager."""

  def testCollectionCallback(self):
    """Test the CollectionCallback."""
    self.StartPatcher(mock.patch('os.walk'))
    os.walk.return_value = [['/checkdir/', [], ['test_check.py']]]

    myobj = TestHealthCheck()
    manager.ImportCheckfile = mock.Mock(
        return_value=[TEST_SERVICE_NAME, [myobj], 100])
    cfm = manager.CheckFileManager(checkdir=CHECKDIR)
    cfm.CollectionCallback()

    manager.ImportCheckfile.assert_called_once_with('/checkdir/test_check.py')

    self.assertTrue(TEST_SERVICE_NAME in cfm.service_checks)
    self.assertEquals(cfm.service_checks[TEST_SERVICE_NAME],
                      {myobj.__class__.__name__: (100, myobj)})

  def testCollectionCallbackNoChecks(self):
    """Test the CollectionCallback with no valid check files."""
    self.StartPatcher(mock.patch('os.walk'))
    os.walk.return_value = [['/checkdir/', [], ['test.py']]]

    manager.ImportCheckfile = mock.Mock(return_value=None)
    cfm = manager.CheckFileManager(checkdir=CHECKDIR)
    cfm.CollectionCallback()

    self.assertFalse(manager.ImportCheckfile.called)

    self.assertFalse(TEST_SERVICE_NAME in cfm.service_checks)

  def testStartCollection(self):
    """Test the StartCollection method."""
    plugins.Monitor = mock.Mock()

    cfm = manager.CheckFileManager(checkdir=CHECKDIR)
    cfm.StartCollection()

    self.assertTrue(plugins.Monitor.called)

  def testUpdateExisting(self):
    """Test update when a health check exists and is not stale."""
    cfm = manager.CheckFileManager(checkdir=CHECKDIR)

    myobj = TestHealthCheck()

    cfm.service_checks[TEST_SERVICE_NAME] = {myobj.__class__.__name__:
                                             (TEST_MTIME, myobj)}

    myobj2 = TestHealthCheck()
    cfm.Update(TEST_SERVICE_NAME, [myobj2], TEST_MTIME)
    self.assertTrue(TEST_SERVICE_NAME in cfm.service_checks)
    self.assertEquals(cfm.service_checks[TEST_SERVICE_NAME],
                      {myobj.__class__.__name__: (TEST_MTIME, myobj)})


  def testUpdateNonExisting(self):
    """Test adding a new health check to the manager."""
    cfm = manager.CheckFileManager(checkdir=CHECKDIR)
    cfm.service_checks = {}

    myobj = TestHealthCheck()
    cfm.Update(TEST_SERVICE_NAME, [myobj], TEST_MTIME)

    self.assertTrue(TEST_SERVICE_NAME in cfm.service_checks)
    self.assertEquals(cfm.service_checks[TEST_SERVICE_NAME],
                      {myobj.__class__.__name__: (TEST_MTIME, myobj)})


class CheckFileModificationTest(cros_test_lib.MockTempDirTestCase):
  """Unittests for checking when live changes are made to a checkfile."""

  MOBMONITOR_BASENAME = 'chromite'
  MOBMONITOR_REL_CMD = 'bin/mobmonitor'
  CHECKFILE_REL_PATH = 'test_check.py'
  NOTACHECK_REL_PATH = 'notacheck.py'
  CHERRYPY_RESTART_STR = 'ENGINE Restarting because %(checkfile)s changed.'
  CHECKFILE_MOD_ATTEMPTS = 3
  TIMEOUT_SEC = 5

  def CreateFile(self, relpath, filestr):
    """Create a file from a string in the temp dir."""
    abspath = os.path.join(self.checkdir, relpath)
    osutils.WriteFile(abspath, filestr)
    return abspath

  def RunCheckfileMod(self, expect_handler, modpath, modfilestr):
    """Test Mob* Monitor restart behaviour with checkfile modification."""
    # Retry the test several times, each time with more relaxed timeouts,
    # to try to control for flakiness as these testcases are dependent
    # on cherrypy startup time and module change detection time.
    for attempt in range(1, self.CHECKFILE_MOD_ATTEMPTS + 1):
      # This target should appear in the output if a checkfile is changed.
      target = self.CHERRYPY_RESTART_STR % {'checkfile':
                                            os.path.join(self.checkdir,
                                                         modpath)}

      # Start the Mob* Monitor in a separate thread. The timeout
      # is how long we will wait to join the thread/wait for output
      # after we have modified the file.
      mobmon = RunCommand(self.cmd, self.TIMEOUT_SEC * attempt)
      mobmon.start()

      # Wait for the monitor to start up fully, then update the file.
      time.sleep(self.TIMEOUT_SEC * attempt)
      self.checkfile = self.CreateFile(modpath, modfilestr)

      # Test whether the target is contained in output and if it
      # matches the expectation.
      if expect_handler(target in mobmon.Stop()):
        return True

    # The test failed.
    return False

  def setUp(self):
    """Setup the check directory and the Mob* Monitor process."""
    # Create the test check directory and the test files.
    self.checkdir = self.tempdir
    self.checkfile = self.CreateFile(self.CHECKFILE_REL_PATH,
                                     CHECKFILE_MANY_SIMPLE)
    self.notacheck = self.CreateFile(self.NOTACHECK_REL_PATH,
                                     NOT_A_CHECKFILE)

    # Setup the Mob* Monitor command.
    path = os.path.abspath(__file__)
    while os.path.basename(path) != self.MOBMONITOR_BASENAME:
      path = os.path.dirname(path)
    path = os.path.join(path, self.MOBMONITOR_REL_CMD)
    self.cmd = [path, '-d', self.checkdir]

  def testModifyCheckfile(self):
    """Test restart behaviour when modifying an imported checkfile."""
    expect_handler = lambda x: x == True

    self.assertTrue(self.RunCheckfileMod(expect_handler,
                                         self.CHECKFILE_REL_PATH,
                                         CHECKFILE_MANY_SIMPLE_ONE_BAD))

  def testModifyNotACheckfile(self):
    """Test that no restart occurs when a non-checkfile is modified."""
    expect_handler = lambda x: x == False

    self.assertTrue(self.RunCheckfileMod(expect_handler,
                                         self.NOTACHECK_REL_PATH,
                                         ANOTHER_NOT_A_CHECKFILE))
