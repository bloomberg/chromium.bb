# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import multiprocessing
import os
import Queue
import select
import threading


class LogReader(object):
  """Helper class used to read a debug log stream on a background thread.
  By default all log messages are stored in an internal buffer. After
  RedirectTo() is called all logs are redirected to the specified stream.
  Optionally the caller can give onwnership of the log process to the reader.
  That process will be killed automatically when the log reader is closed.
  """

  def __init__(self, process, stream):
    self._process = process
    self._stream = stream
    self._buffer = Queue.Queue()

    # Start a background thread that redirects the stream to self._buffer.
    self._StartThread(lambda line: self._buffer.put(line))

  def _StartThread(self, on_message_received):
    self._quit_pipe, thread_quit_pipe = multiprocessing.Pipe()

    self._thread = threading.Thread(target=self._ReadThread,
                                    args=(thread_quit_pipe,
                                          on_message_received))
    self._thread.daemon = True
    self._thread.start()

  def __enter__(self):
    return self

  def __exit__(self, exception_type, exception_value, traceback):
    self.Close()

  def __del__(self):
    self.Close()

  def _ReadThread(self, quit_pipe, on_message_received):
    """Main function for the background thread that reads |self._stream| and
    calls |on_message_received| for each line."""

    poll = select.poll()
    poll.register(self._stream.fileno(), select.POLLIN)
    poll.register(quit_pipe.fileno(), select.POLLIN)

    quit = False
    while not quit:
      events = poll.poll()
      for fileno, event in events:
        if fileno == quit_pipe.fileno():
          quit = True
          break

        assert fileno == self._stream.fileno()
        if event & select.POLLIN:
          on_message_received(self._stream.readline())
        elif event & select.POLLHUP:
          quit = True

  def _StopThread(self):
    if self._thread:
      try:
        self._quit_pipe.send("quit")
      except IOError:
        # The thread has already quit and closed the other end of the pipe.
        pass
      self._thread.join();
      self._thread = None

  def Close(self):
    self._StopThread()
    if self._process:
      self._process.kill()
      self._process = None

  def RedirectTo(self, stream):
    self._StopThread()

    # Drain and reset the buffer.
    while True:
      try:
        line = self._buffer.get_nowait()
        stream.write(line)
      except Queue.Empty:
        break

    # Restart the thread to write stream to stdout.
    self._StartThread(lambda line: stream.write(line))
