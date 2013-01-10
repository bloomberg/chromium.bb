#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A simple native client in python.
# All this client does is echo the text it recieves back at the extension.

import sys
import struct

MESSAGE_TYPE_SEND_MESSAGE_REQUEST = 0
MESSAGE_TYPE_SEND_MESSAGE_RESPONSE = 1
MESSAGE_TYPE_CONNECT = 2
MESSAGE_TYPE_CONNECT_MESSAGE = 3

def Main():
  message_number = 0

  while 1:
    # Read the message type (first 4 bytes).
    type_bytes = sys.stdin.read(4)

    if len(type_bytes) == 0:
      break

    message_type = struct.unpack('i', type_bytes)[0]

    # Read the message length (4 bytes).
    text_length = struct.unpack('i', sys.stdin.read(4))[0]

    # Read the text (JSON object) of the message.
    text = sys.stdin.read(text_length).decode('utf-8')

    message_number += 1

    response = '{{"id": {0}, "echo": {1}}}'.format(message_number,
                                                  text).encode('utf-8')

    # Choose the correct message type for the response.
    if message_type == MESSAGE_TYPE_SEND_MESSAGE_REQUEST:
      response_type = MESSAGE_TYPE_SEND_MESSAGE_RESPONSE
    else:
      response_type = MESSAGE_TYPE_CONNECT_MESSAGE

    try:
      sys.stdout.write(struct.pack("II", response_type, len(response)))
      sys.stdout.write(response)
      sys.stdout.flush()
    except IOError:
      break

if __name__ == '__main__':
  Main()
