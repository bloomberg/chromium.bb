# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This module is handler for incoming data to the pywebsocket standalone server
# (source is in http://code.google.com/p/pywebsocket/source/browse/trunk/src/).
# It follows the conventions of the pywebsocket server and in our case receives
# and stores incoming frames to disk.

import Queue
import os
import threading

_NUMBER_OF_WRITER_THREADS = 10

# I couldn't think of other way to handle this but through a global variable
g_frame_number_counter = 0
g_frames_queue = Queue.Queue()


def web_socket_do_extra_handshake(request):
  pass  # Always accept.


def web_socket_transfer_data(request):
  while True:
    data = request.ws_stream.receive_message()
    if data is None:
      return

    # We assume we will receive only frames, i.e. binary data
    global g_frame_number_counter
    frame_number = str(g_frame_number_counter)
    g_frame_number_counter += 1
    g_frames_queue.put((frame_number, data))


class FrameWriterThread(threading.Thread):
  """Writes received frames to disk.

  The frames are named in the format frame_xxxx, where xxxx is the 0-padded
  frame number. The frames and their numbers are obtained from a synchronized
  queue. The frames are written in the directory specified in the environment
  variable PYWS_DIR_FOR_HANDLER_OUTPUT.
  """
  def __init__(self, queue):
    threading.Thread.__init__(self)
    self.queue = queue

  def run(self):
    while True:
      frame_number, frame_data = self.queue.get()
      file_name = 'frame_' + frame_number.zfill(4)
      file_name = os.path.join(os.environ['PYWS_DIR_FOR_HANDLER_OUTPUT'],
                               file_name)
      frame = open(file_name, "wb")
      frame.write(frame_data)
      frame.close()
      self.queue.task_done()


def start_threads():
  for i in range(_NUMBER_OF_WRITER_THREADS):
    t = FrameWriterThread(g_frames_queue)
    t.setDaemon(True)
    t.start()
  g_frames_queue.join()


# This handler's entire code is imported as 'it is' and then incorporated in the
# code of the standalone pywebsocket server. If we put this start_threads() call
# inside a if __name__ == '__main__' clause it wouldn't run this code at all
# (tested).
start_threads()