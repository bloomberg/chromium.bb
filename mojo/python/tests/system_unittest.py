# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random
import sys
import time
import unittest

# pylint: disable=F0401
from mojo.embedder import init as init_embedder
from mojo import system

DATA_SIZE = 1024


def get_random_buffer(size):
  random.seed(size)
  return bytearray(''.join(chr(random.randint(0, 255)) for i in xrange(size)))


class BaseMojoTest(unittest.TestCase):

  def setUp(self):
    init_embedder()


class CoreTest(BaseMojoTest):

  def test_results(self):
    self.assertEquals(system.RESULT_OK, 0)
    self.assertLess(system.RESULT_CANCELLED, 0)
    self.assertLess(system.RESULT_UNKNOWN, 0)
    self.assertLess(system.RESULT_INVALID_ARGUMENT, 0)
    self.assertLess(system.RESULT_DEADLINE_EXCEEDED, 0)
    self.assertLess(system.RESULT_NOT_FOUND, 0)
    self.assertLess(system.RESULT_ALREADY_EXISTS, 0)
    self.assertLess(system.RESULT_PERMISSION_DENIED, 0)
    self.assertLess(system.RESULT_RESOURCE_EXHAUSTED, 0)
    self.assertLess(system.RESULT_FAILED_PRECONDITION, 0)
    self.assertLess(system.RESULT_ABORTED, 0)
    self.assertLess(system.RESULT_OUT_OF_RANGE, 0)
    self.assertLess(system.RESULT_UNIMPLEMENTED, 0)
    self.assertLess(system.RESULT_INTERNAL, 0)
    self.assertLess(system.RESULT_UNAVAILABLE, 0)
    self.assertLess(system.RESULT_DATA_LOSS, 0)
    self.assertLess(system.RESULT_BUSY, 0)
    self.assertLess(system.RESULT_SHOULD_WAIT, 0)

  def test_constants(self):
    self.assertGreaterEqual(system.DEADLINE_INDEFINITE, 0)
    self.assertGreaterEqual(system.HANDLE_SIGNAL_NONE, 0)
    self.assertGreaterEqual(system.HANDLE_SIGNAL_READABLE, 0)
    self.assertGreaterEqual(system.HANDLE_SIGNAL_WRITABLE, 0)
    self.assertGreaterEqual(system.WRITE_MESSAGE_FLAG_NONE, 0)
    self.assertGreaterEqual(system.READ_MESSAGE_FLAG_NONE, 0)
    self.assertGreaterEqual(system.READ_MESSAGE_FLAG_MAY_DISCARD, 0)
    self.assertGreaterEqual(system.WRITE_DATA_FLAG_NONE, 0)
    self.assertGreaterEqual(system.WRITE_DATA_FLAG_ALL_OR_NONE, 0)
    self.assertGreaterEqual(system.READ_DATA_FLAG_NONE, 0)
    self.assertGreaterEqual(system.READ_DATA_FLAG_ALL_OR_NONE, 0)
    self.assertGreaterEqual(system.READ_DATA_FLAG_DISCARD, 0)
    self.assertGreaterEqual(system.READ_DATA_FLAG_QUERY, 0)
    self.assertGreaterEqual(system.MAP_BUFFER_FLAG_NONE, 0)

  def test_get_time_ticks_now(self):
    pt1 = time.time()
    v1 = system.get_time_ticks_now()
    time.sleep(1e-3)
    v2 = system.get_time_ticks_now()
    pt2 = time.time()
    self.assertGreater(v1, 0)
    self.assertGreater(v2, v1 + 1000)
    self.assertGreater(1e6 * (pt2 - pt1), v2 - v1)

  def _test_handles_creation(self, *args):
    for handle in args:
      self.assertTrue(handle.is_valid())
      handle.close()
      self.assertFalse(handle.is_valid())

  def _test_message_handle_creation(self, handles):
    self._test_handles_creation(handles.handle0, handles.handle1)

  def test_create_message_pipe(self):
    self._test_message_handle_creation(system.MessagePipe())

  def test_create_message_pipe_with_none_options(self):
    self._test_message_handle_creation(system.MessagePipe(None))

  def test_create_message_pipe_with_options(self):
    self._test_message_handle_creation(
        system.MessagePipe(system.CreateMessagePipeOptions()))

  def test_wait_over_message_pipe(self):
    handles = system.MessagePipe()
    handle = handles.handle0

    self.assertEquals(system.RESULT_OK, handle.wait(
        system.HANDLE_SIGNAL_WRITABLE, system.DEADLINE_INDEFINITE))
    self.assertEquals(system.RESULT_DEADLINE_EXCEEDED,
                      handle.wait(system.HANDLE_SIGNAL_READABLE, 0))

    handles.handle1.write_message()

    self.assertEquals(
        system.RESULT_OK,
        handle.wait(
            system.HANDLE_SIGNAL_READABLE,
            system.DEADLINE_INDEFINITE))

  def test_wait_over_many_message_pipe(self):
    handles = system.MessagePipe()
    handle0 = handles.handle0
    handle1 = handles.handle1

    self.assertEquals(
        0,
        system.wait_many(
            [(handle0, system.HANDLE_SIGNAL_WRITABLE),
             (handle1, system.HANDLE_SIGNAL_WRITABLE)],
            system.DEADLINE_INDEFINITE))
    self.assertEquals(
        system.RESULT_DEADLINE_EXCEEDED,
        system.wait_many(
            [(handle0, system.HANDLE_SIGNAL_READABLE),
             (handle1, system.HANDLE_SIGNAL_READABLE)], 0))

    handle0.write_message()

    self.assertEquals(
        1,
        system.wait_many(
            [(handle0, system.HANDLE_SIGNAL_READABLE),
             (handle1, system.HANDLE_SIGNAL_READABLE)],
            system.DEADLINE_INDEFINITE))

  def test_send_bytes_over_message_pipe(self):
    handles = system.MessagePipe()
    data = get_random_buffer(DATA_SIZE)
    handles.handle0.write_message(data)
    (res, buffers, next_message) = handles.handle1.read_message()
    self.assertEquals(system.RESULT_RESOURCE_EXHAUSTED, res)
    self.assertEquals(None, buffers)
    self.assertEquals((DATA_SIZE, 0), next_message)
    result = bytearray(DATA_SIZE)
    (res, buffers, next_message) = handles.handle1.read_message(result)
    self.assertEquals(system.RESULT_OK, res)
    self.assertEquals(None, next_message)
    self.assertEquals((data, []), buffers)

  def test_send_empty_data_over_message_pipe(self):
    handles = system.MessagePipe()
    handles.handle0.write_message(None)
    (res, buffers, next_message) = handles.handle1.read_message()

    self.assertEquals(system.RESULT_OK, res)
    self.assertEquals(None, next_message)
    self.assertEquals((None, []), buffers)

  def test_send_handle_over_message_pipe(self):
    handles = system.MessagePipe()
    handles_to_send = system.MessagePipe()
    handles.handle0.write_message(handles=[handles_to_send.handle0,
                                           handles_to_send.handle1])
    (res, buffers, next_message) = handles.handle1.read_message(
        max_number_of_handles=2)

    self.assertFalse(handles_to_send.handle0.is_valid())
    self.assertFalse(handles_to_send.handle1.is_valid())
    self.assertEquals(system.RESULT_OK, res)
    self.assertEquals(None, next_message)
    self.assertEquals(None, buffers[0])
    self.assertEquals(2, len(buffers[1]))

    handles = buffers[1]
    for handle in handles:
      self.assertTrue(handle.is_valid())
      (res, buffers, next_message) = handle.read_message()
      self.assertEquals(system.RESULT_SHOULD_WAIT, res)

    for handle in handles:
      handle.write_message()

    for handle in handles:
      (res, buffers, next_message) = handle.read_message()
      self.assertEquals(system.RESULT_OK, res)

  def _test_data_handle_creation(self, handles):
    self._test_handles_creation(
        handles.producer_handle, handles.consumer_handle)

  def test_create_data_pipe(self):
    self._test_data_handle_creation(system.DataPipe())

  def test_create_data_pipe_with_none_options(self):
    self._test_data_handle_creation(system.DataPipe(None))

  def test_create_data_pipe_with_default_options(self):
    self._test_data_handle_creation(
        system.DataPipe(system.CreateDataPipeOptions()))

  def test_create_data_pipe_with_discard_flag(self):
    options = system.CreateDataPipeOptions()
    options.flags = system.CreateDataPipeOptions.FLAG_MAY_DISCARD
    self._test_data_handle_creation(system.DataPipe(options))

  def test_create_data_pipe_with_element_size(self):
    options = system.CreateDataPipeOptions()
    options.element_num_bytes = 5
    self._test_data_handle_creation(system.DataPipe(options))

  def test_create_data_pipe_with_capacity(self):
    options = system.CreateDataPipeOptions()
    options.element_capacity_num_bytes = DATA_SIZE
    self._test_data_handle_creation(system.DataPipe(options))

  def test_create_data_pipe_with_incorrect_parameters(self):
    options = system.CreateDataPipeOptions()
    options.element_num_bytes = 5
    options.capacity_num_bytes = DATA_SIZE
    with self.assertRaises(system.MojoException) as cm:
      self._test_data_handle_creation(system.DataPipe(options))
    self.assertEquals(system.RESULT_INVALID_ARGUMENT, cm.exception.mojo_result)

  def test_send_empty_data_over_data_pipe(self):
    pipes = system.DataPipe()
    self.assertEquals((system.RESULT_OK, 0), pipes.producer_handle.write_data())
    self.assertEquals(
        (system.RESULT_OK, None), pipes.consumer_handle.read_data())

  def test_send_data_over_data_pipe(self):
    pipes = system.DataPipe()
    data = get_random_buffer(DATA_SIZE)
    self.assertEquals((system.RESULT_OK, DATA_SIZE),
                      pipes.producer_handle.write_data(data))
    self.assertEquals((system.RESULT_OK, data),
                      pipes.consumer_handle.read_data(bytearray(DATA_SIZE)))

  def test_two_phase_write_on_data_pipe(self):
    pipes = system.DataPipe()
    (res, buf) = pipes.producer_handle.begin_write_data(DATA_SIZE)
    self.assertEquals(system.RESULT_OK, res)
    self.assertGreaterEqual(len(buf.buffer), DATA_SIZE)
    data = get_random_buffer(DATA_SIZE)
    buf.buffer[0:DATA_SIZE] = data
    self.assertEquals(system.RESULT_OK, buf.end(DATA_SIZE))
    self.assertEquals((system.RESULT_OK, data),
                      pipes.consumer_handle.read_data(bytearray(DATA_SIZE)))

  def test_two_phase_read_on_data_pipe(self):
    pipes = system.DataPipe()
    data = get_random_buffer(DATA_SIZE)
    self.assertEquals((system.RESULT_OK, DATA_SIZE),
                      pipes.producer_handle.write_data(data))
    (res, buf) = pipes.consumer_handle.begin_read_data()
    self.assertEquals(system.RESULT_OK, res)
    self.assertEquals(DATA_SIZE, len(buf.buffer))
    self.assertEquals(data, buf.buffer)
    self.assertEquals(system.RESULT_OK, buf.end(DATA_SIZE))

  def test_create_shared_buffer(self):
    self._test_handles_creation(system.create_shared_buffer(DATA_SIZE))

  def test_create_shared_buffer_with_none_options(self):
    self._test_handles_creation(system.create_shared_buffer(DATA_SIZE, None))

  def test_create_shared_buffer_with_default_options(self):
    self._test_handles_creation(
        system.create_shared_buffer(
            DATA_SIZE,
            system.CreateSharedBufferOptions()))

  def test_duplicate_shared_buffer(self):
    handle = system.create_shared_buffer(DATA_SIZE)
    self._test_handles_creation(handle.duplicate())

  def test_duplicate_shared_buffer_with_none_options(self):
    handle = system.create_shared_buffer(DATA_SIZE)
    self._test_handles_creation(handle.duplicate(None))

  def test_duplicate_shared_buffer_with_default_options(self):
    handle = system.create_shared_buffer(DATA_SIZE)
    self._test_handles_creation(
        handle.duplicate(system.DuplicateSharedBufferOptions()))

  def test_send_bytes_over_shared_buffer(self):
    handle = system.create_shared_buffer(DATA_SIZE)
    duplicated = handle.duplicate()
    data = get_random_buffer(DATA_SIZE)
    (res1, buf1) = handle.map(0, DATA_SIZE)
    (res2, buf2) = duplicated.map(0, DATA_SIZE)
    self.assertEquals(system.RESULT_OK, res1)
    self.assertEquals(system.RESULT_OK, res2)
    self.assertEquals(DATA_SIZE, len(buf1.buffer))
    self.assertEquals(DATA_SIZE, len(buf2.buffer))
    self.assertEquals(buf1.buffer, buf2.buffer)

    buf1.buffer[:] = data
    self.assertEquals(data, buf1.buffer)
    self.assertEquals(data, buf2.buffer)
    self.assertEquals(buf1.buffer, buf2.buffer)


if __name__ == '__main__':
  suite = unittest.TestLoader().loadTestsFromTestCase(CoreTest)
  test_results = unittest.TextTestRunner(verbosity=0).run(suite)
  if not test_results.wasSuccessful():
    sys.exit(1)
  sys.exit(0)
