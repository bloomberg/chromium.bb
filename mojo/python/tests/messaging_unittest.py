# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

# pylint: disable=F0401
import mojo.embedder
from mojo.bindings import messaging
from mojo import system


class _ForwardingMessageReceiver(messaging.MessageReceiver):

  def __init__(self, callback):
    self._callback = callback

  def Accept(self, message):
    self._callback(message)
    return True


class _ForwardingConnectionErrorHandler(messaging.ConnectionErrorHandler):

  def __init__(self, callback):
    self._callback = callback

  def OnError(self, result):
    self._callback(result)


class MessagingTest(unittest.TestCase):

  def setUp(self):
    mojo.embedder.Init()
    self.loop = system.RunLoop()
    self.received_messages = []
    self.received_errors = []
    def _OnMessage(message):
      self.received_messages.append(message)
    def _OnError(result):
      self.received_errors.append(result)
    handles = system.MessagePipe()
    self.connector = messaging.Connector(handles.handle1)
    self.connector.SetIncomingMessageReceiver(
        _ForwardingMessageReceiver(_OnMessage))
    self.connector.SetErrorHandler(
        _ForwardingConnectionErrorHandler(_OnError))
    self.connector.Start()
    self.handle = handles.handle0


  def tearDown(self):
    self.connector = None
    self.handle = None
    self.loop = None

  def testConnectorRead(self):
    self.handle.WriteMessage()
    self.loop.RunUntilIdle()
    self.assertTrue(self.received_messages)
    self.assertFalse(self.received_errors)

  def testConnectorWrite(self):
    self.connector.Accept(messaging.Message())
    (result, _, _) = self.handle.ReadMessage()
    self.assertEquals(result, system.RESULT_OK)
    self.assertFalse(self.received_errors)

  def testConnectorCloseRemoteHandle(self):
    self.handle.Close()
    self.loop.RunUntilIdle()
    self.assertFalse(self.received_messages)
    self.assertTrue(self.received_errors)
    self.assertEquals(self.received_errors[0],
                      system.RESULT_FAILED_PRECONDITION)

  def testConnectorDeleteConnector(self):
    self.connector = None
    (result, _, _) = self.handle.ReadMessage()
    self.assertEquals(result, system.RESULT_FAILED_PRECONDITION)
