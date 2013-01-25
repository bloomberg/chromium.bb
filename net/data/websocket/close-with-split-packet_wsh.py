# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import struct

from mod_pywebsocket import handshake
from mod_pywebsocket import stream


def web_socket_do_extra_handshake(_request):
  pass


def web_socket_transfer_data(request):
  line = request.ws_stream.receive_message()
  if line is None:
    return
  if isinstance(line, unicode):
    request.ws_stream.send_message(line, binary=False)
  else:
    request.ws_stream.send_message(line, binary=True)


def web_socket_passive_closing_handshake(request):
  code = struct.pack('!H', 1000)
  packet = stream.create_close_frame(code + 'split test'.encode('utf-8'))
  request.connection.write(packet[:1])
  request.connection.write(packet[1:])
  raise handshake.AbortedByUserException('Abort the connection')
