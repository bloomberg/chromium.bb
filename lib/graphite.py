# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Interface for sending data to Graphite."""

from __future__ import print_function

import socket

from chromite.lib import cros_build_lib

CARBON_SERVER = 'chromeos-stats.corp.google.com'
CARBON_PORT = 2003

def SendToCarbon(lines, dryrun=False, process_queue=20):
  """Send data to the statsd/graphite server.

  Example of a line "autotest.scheduler.running_agents_5m 300"
  5m is the frequency we are sampling (It is not required but it adds clarity
  to the metric).

  Args:
    lines: A list of lines of the format "category value"
    dryrun: Print out what you would send but do not send anything.
      [default: False]
    process_queue: How many lines to send to the statsd server at a
      time. [defualt: 20]
  """
  sock = socket.socket()

  try:
    sock.connect((CARBON_SERVER, CARBON_PORT))
  except Exception:
    cros_build_lib.Error('Failed to connect to Carbon.')
    raise

  slices = [lines[i:i+process_queue]
            for i in range(0, len(lines), process_queue)]
  for lines in slices:
    data = '\n'.join(lines) + '\n'
    if dryrun:
      cros_build_lib.Info('Not sending to Graphite via Carbon:\n%s', data)
    else:
      cros_build_lib.Debug('Sending to Graphite via Carbon:\n%s', data)
      sock.sendall(data)
