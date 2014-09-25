# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility classes to handle sending and receiving messages."""


import weakref

# pylint: disable=F0401
import mojo.system as system


class Message(object):
  """A message for a message pipe. This contains data and handles."""

  def __init__(self, data=None, handles=None):
    self.data = data
    self.handles = handles


class MessageReceiver(object):
  """A class which implements this interface can receive Message objects."""

  def Accept(self, message):
    """
    Receive a Message. The MessageReceiver is allowed to mutate the message.

    Args:
      message: the received message.

    Returns:
      True if the message has been handled, False otherwise.
    """
    raise NotImplementedError()


class MessageReceiverWithResponder(MessageReceiver):
  """
  A MessageReceiver that can also handle the response message generated from the
  given message.
  """

  def AcceptWithResponder(self, message, responder):
    """
    A variant on Accept that registers a MessageReceiver (known as the
    responder) to handle the response message generated from the given message.
    The responder's Accept method may be called as part of the call to
    AcceptWithResponder, or some time after its return.

    Args:
      message: the received message.
      responder: the responder that will receive the response.

    Returns:
      True if the message has been handled, False otherwise.
    """
    raise NotImplementedError()


class ConnectionErrorHandler(object):
  """
  A ConnectionErrorHandler is notified of an error happening while using the
  bindings over message pipes.
  """

  def OnError(self, result):
    raise NotImplementedError()


class Connector(MessageReceiver):
  """
  A Connector owns a message pipe and will send any received messages to the
  registered MessageReceiver. It also acts as a MessageReceiver and will send
  any message through the handle.

  The method Start must be called before the Connector will start listening to
  incoming messages.
  """

  def __init__(self, handle):
    MessageReceiver.__init__(self)
    self._handle = handle
    self._cancellable = None
    self._incoming_message_receiver = None
    self._error_handler = None

  def __del__(self):
    if self._cancellable:
      self._cancellable()

  def SetIncomingMessageReceiver(self, message_receiver):
    """
    Set the MessageReceiver that will receive message from the owned message
    pipe.
    """
    self._incoming_message_receiver = message_receiver

  def SetErrorHandler(self, error_handler):
    """
    Set the ConnectionErrorHandler that will be notified of errors on the owned
    message pipe.
    """
    self._error_handler = error_handler

  def Start(self):
    assert not self._cancellable
    self._RegisterAsyncWaiterForRead()

  def Accept(self, message):
    result = self._handle.WriteMessage(message.data, message.handles)
    return result == system.RESULT_OK

  def _OnAsyncWaiterResult(self, result):
    self._cancellable = None
    if result == system.RESULT_OK:
      self._ReadOutstandingMessages()
    else:
      self._OnError(result)

  def _OnError(self, result):
    assert not self._cancellable
    if self._error_handler:
      self._error_handler.OnError(result)

  def _RegisterAsyncWaiterForRead(self) :
    assert not self._cancellable
    self._cancellable = self._handle.AsyncWait(
        system.HANDLE_SIGNAL_READABLE,
        system.DEADLINE_INDEFINITE,
        _WeakCallback(self._OnAsyncWaiterResult))

  def _ReadOutstandingMessages(self):
    result = system.RESULT_OK
    while result == system.RESULT_OK:
      result = _ReadAndDispatchMessage(self._handle,
                                       self._incoming_message_receiver)
    if result == system.RESULT_SHOULD_WAIT:
      self._RegisterAsyncWaiterForRead()
      return
    self._OnError(result)


def _WeakCallback(callback):
  func = callback.im_func
  self = callback.im_self
  if not self:
    return callback
  weak_self = weakref.ref(self)
  def Callback(*args, **kwargs):
    self = weak_self()
    if self:
      return func(self, *args, **kwargs)
  return Callback


def _ReadAndDispatchMessage(handle, message_receiver):
  (result, _, sizes) = handle.ReadMessage()
  if result == system.RESULT_OK and message_receiver:
    message_receiver.Accept(Message(bytearray(), []))
  if result != system.RESULT_RESOURCE_EXHAUSTED:
    return result
  (result, data, _) = handle.ReadMessage(bytearray(sizes[0]))
  if result == system.RESULT_OK and message_receiver:
    message_receiver.Accept(Message(data[0], data[1]))
  return result

