# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

# pylint: disable=F0401
import mojo.embedder
from mojo import system


class AsyncWaitTest(unittest.TestCase):

  def setUp(self):
    mojo.embedder.Init()
    self.loop = system.RunLoop()
    self.array = []
    self.handles = system.MessagePipe()
    self.cancel = self.handles.handle0.AsyncWait(system.HANDLE_SIGNAL_READABLE,
                                                 system.DEADLINE_INDEFINITE,
                                                 self.OnResult)
    self.loop.PostDelayedTask(self.WriteToHandle, 100)

  def tearDown(self):
    self.handles = None
    self.array = None
    self.loop = None

  def OnResult(self, value):
    self.array.append(value)

  def WriteToHandle(self):
    self.handles.handle1.WriteMessage()

  def testAsyncWait(self):
    self.loop.RunUntilIdle()
    self.assertEquals(len(self.array), 1)
    self.assertEquals(system.RESULT_OK, self.array[0])
    self.cancel()

  def testAsyncWaitCancel(self):
    self.loop.PostDelayedTask(self.cancel, 50)
    self.loop.RunUntilIdle()
    self.assertEquals(len(self.array), 0)
    self.cancel()

  def testAsyncWaitImmediateCancel(self):
    self.cancel()
    self.loop.RunUntilIdle()
    self.assertEquals(len(self.array), 0)
    self.cancel()
