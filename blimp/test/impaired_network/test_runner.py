#!/usr/bin/env python

# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import multiprocessing
import os
import pwd
import subprocess

from twisted.internet import reactor

from packet_queue import monitoring
from packet_queue import nfqueue
from packet_queue import simulation


ENGINE_PORT = 25467
MTU = 2048


def parse_args():
  parser = argparse.ArgumentParser()

  parser.add_argument(
      '--target', type=str, required=True,
      help='path of the test binary to run')
  parser.add_argument(
      '--gtest_filter', type=str, default='EngineBrowserTest.LoadUrl',
      help='which specific test to run')
  parser.add_argument(
      '-w', '--bandwidth', type=int, default=-1,
      help='bandwidth cap in bytes/second, unlimited by default')
  parser.add_argument(
      '-b', '--buffer', type=int, default=-1,
      help='buffer size in bytes, unlimited by default')
  parser.add_argument(
      '-d', '--delay', type=float, default=0.0,
      help='one-way packet delay in seconds')
  parser.add_argument(
      '-l', '--loss', type=float, default=0.0,
      help='probability of random packet loss')

  return parser.parse_args()


def run_packet_queue(params, ready_event):
  """Runs the packet queue with parameters from command-line args.

  This function runs the Twisted reactor, which blocks until the process is
  interrupted.
  """
  event_log = monitoring.EventLog()
  pipes = simulation.PipePair(params, event_log)

  nfqueue.configure('tcp', ENGINE_PORT, pipes, 'lo')

  reactor.callWhenRunning(ready_event.set)
  reactor.addSystemEventTrigger(
      'before', 'shutdown', report_stats, event_log)
  reactor.run()


def report_stats(event_log):
  """Prints accumulated network stats to stdout."""
  dropped = 0
  delivered = 0

  for event in event_log.get_pending():
    if event['type'] == 'drop':
      dropped += event['value']
    elif event['type'] == 'deliver':
      delivered += event['value']

  print '%i bytes delivered' % delivered
  print '%i bytes dropped' % dropped


def set_chrome_test_user():
  """If the SUDO_USER environment variable exists, sets the user to that.

  This is required because Chrome browser tests won't run as root.

  If SUDO_USER isn't present, we might be running as a non-root user with
  the CAP_NET_ADMIN capability, so the user won't change.
  """
  sudo_user = os.getenv('SUDO_USER')
  if sudo_user:
    uid = pwd.getpwnam(sudo_user).pw_uid
    os.setuid(uid)


def main():
  """Runs a Blimp test target with network impairment.

  This works by forking to run the packet queue, and then running the test
  binary in a subprocess when the packet queue is ready.
  """
  args = parse_args()

  simulation_params = {
      'bandwidth': args.bandwidth,
      'buffer': args.buffer,
      'delay': args.delay,
      'loss': args.loss,
  }

  ready_event = multiprocessing.Event()
  packet_queue = multiprocessing.Process(
      target=run_packet_queue, args=(simulation_params, ready_event))

  if subprocess.call(['ip', 'link', 'set', 'dev', 'lo', 'mtu', str(MTU)]):
    raise OSError('Failed to set MTU')

  packet_queue.start()
  ready_event.wait(timeout=3)

  subprocess.call([
      args.target,
      '--disable-setuid-sandbox',
      '--single_process',
      '--engine-port=%i' % ENGINE_PORT,
      '--gtest_filter=%s' % args.gtest_filter,
  ], preexec_fn=set_chrome_test_user)

  packet_queue.terminate()


if __name__ == '__main__':
  main()
