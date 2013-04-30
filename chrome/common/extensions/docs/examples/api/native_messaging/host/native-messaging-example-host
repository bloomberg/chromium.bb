#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A simple native client in python.
# All this client does is echo the text it receives back at the extension.

import sys
import struct

def Main():
  message_number = 0

  while 1:
    # Read the message type (first 4 bytes).
    text_length_bytes = sys.stdin.read(4)

    if len(text_length_bytes) == 0:
      break

    # Read the message length (4 bytes).
    text_length = struct.unpack('i', text_length_bytes)[0]

    # Read the text (JSON object) of the message.
    text = sys.stdin.read(text_length).decode('utf-8')

    message_number += 1

    response = '{{"id": {0}, "echo": {1}}}'.format(message_number,
                                                   text).encode('utf-8')

    try:
      sys.stdout.write(struct.pack("I", len(response)))
      sys.stdout.write(response)
      sys.stdout.flush()
    except IOError:
      break

if __name__ == '__main__':
  Main()
