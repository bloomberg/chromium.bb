# -*- coding: utf-8 -*-
# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Signal related functionality."""

from __future__ import print_function

import signal
import contextlib


def RelaySignal(handler, signum, frame):
  """Notify a listener returned from getsignal of receipt of a signal.

  Returns:
    True if it was relayed to the target, False otherwise.
    False in particular occurs if the target isn't relayable.
  """
  if handler in (None, signal.SIG_IGN):
    return True
  elif handler == signal.SIG_DFL:
    # This scenario is a fairly painful to handle fully, thus we just
    # state we couldn't handle it and leave it to client code.
    return False
  handler(signum, frame)
  return True


@contextlib.contextmanager
def DeferSignals(*args):
  """Context Manger to defer signals during a critical block.

  If a signal comes in for the masked signals, the original handler
  is ran after the  critical block has exited.

  Args:
    args: Which signals to ignore.  If none are given, defaults to
      SIGINT and SIGTERM.
  """
  signals = args
  if not signals:
    signals = [signal.SIGINT, signal.SIGTERM, signal.SIGALRM]

  # Rather than directly setting the handler, we first pull the handlers, then
  # set the new handler.  The ordering has to be done this way to ensure that
  # if someone passes in a bad signum (or a signal lands prior to starting the
  # critical block), we can restore things to pristine state.
  handlers = dict((signum, signal.getsignal(signum)) for signum in signals)

  received = []
  def handler(signum, frame):
    received.append((signum, frame))

  try:
    for signum in signals:
      signal.signal(signum, handler)

    yield

  finally:
    for signum, original in handlers.items():
      signal.signal(signum, original)

    for signum, frame in received:
      RelaySignal(handlers[signum], signum, frame)


def StrSignal(sig_num):
  """Convert a signal number to the symbolic name

  Note: Some signal number have multiple names, so you might get
  back a confusing result like "SIGIOT|SIGABRT".  Since they have
  the same signal number, it's impossible to say which one is right.

  Args:
    sig_num: The numeric signal you wish to convert

  Returns:
    A string of the signal name(s)
  """
  # Handle realtime signals first since they are unnamed.
  if sig_num >= signal.SIGRTMIN and sig_num < signal.SIGRTMAX:
    return 'SIGRT_%i' % sig_num

  # Probe the module looking for matching signal constant.
  sig_names = []
  for name, num in signal.__dict__.items():
    if name.startswith('SIG') and num == sig_num:
      sig_names.append(name)
  if sig_names:
    return '|'.join(sig_names)
  else:
    return 'SIG_%i' % sig_num
