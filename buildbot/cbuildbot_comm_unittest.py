#!/usr/bin/python

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Units tests for cbuildbot_comm commands."""

import cbuildbot_comm
import sys
import threading
import time
import unittest

_TEST_CONFIG = {'test_slave' :
                {'master' : False,
                 'hostname' : 'localhost',
                 'important' : True},
                'test_master' :
                {'master' : True,
                 'important' : False
                }
               }

# Reduce timeouts.
cbuildbot_comm._HEARTBEAT_TIMEOUT = 2
cbuildbot_comm._MAX_TIMEOUT = 6

class _MasterSendBadStatus(threading.Thread):

  def __init__(self, test_class):
    threading.Thread.__init__(self)
    self.test_class = test_class

  def run(self):
    # Sleep for heartbeat timeout to let slave start up.
    time.sleep(2)
    return_value = cbuildbot_comm._SendCommand('localhost', 'bad-command',
                                               'args')
    self.test_class.assertEqual(return_value,
                                cbuildbot_comm._STATUS_COMMAND_REJECTED)

class _MasterCheckStatusThread(threading.Thread):

  def __init__(self, config, expected_return, test_class):
    threading.Thread.__init__(self)
    self.config = config
    self.expected_return = expected_return
    self.test_class = test_class

  def run(self):
    return_value = cbuildbot_comm.HaveSlavesCompleted(self.config)
    self.test_class.assertEqual(return_value, self.expected_return)


class CBuildBotCommTest(unittest.TestCase):

  def testSlaveComplete(self):
    print >> sys.stderr, '\n>>> Running testSlaveComplete\n'
    # Master should check statuses in another thread.
    master_thread = _MasterCheckStatusThread(_TEST_CONFIG, True, self)
    master_thread.start()

    return_value = cbuildbot_comm.PublishStatus(
        cbuildbot_comm.STATUS_BUILD_COMPLETE)
    self.assertEqual(return_value, True)

  def testMasterTimeout(self):
    print >> sys.stderr, '\n>>> Running testMasterTimeout\n'
    return_value = cbuildbot_comm.HaveSlavesCompleted(_TEST_CONFIG)
    self.assertEqual(return_value, False)

  def testSlaveTimeout(self):
    print >> sys.stderr, '\n>>> Running testSlaveTimeout\n'
    return_value = cbuildbot_comm.PublishStatus(
        cbuildbot_comm.STATUS_BUILD_COMPLETE)
    self.assertEqual(return_value, False)

  def testSlaveFail(self):
    print >> sys.stderr, '\n>>> Running testSlaveFail\n'
    # Master should check statuses in another thread.
    master_thread = _MasterCheckStatusThread(_TEST_CONFIG, False, self)
    master_thread.start()

    return_value = cbuildbot_comm.PublishStatus(
        cbuildbot_comm.STATUS_BUILD_FAILED)
    self.assertEqual(return_value, True)

  def testBadCommand(self):
    print >> sys.stderr, '\n>>> Running testSendBadCommand\n'
    # Master should check statuses in another thread.
    master_thread = _MasterSendBadStatus(self)
    master_thread.start()

    return_value = cbuildbot_comm.PublishStatus(
        cbuildbot_comm.STATUS_BUILD_COMPLETE)
    self.assertEqual(return_value, False)


if __name__ == '__main__':
  unittest.main()
