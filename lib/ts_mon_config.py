# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for inframon's command-line flag based configuration."""

from __future__ import print_function

import argparse
import contextlib
import multiprocessing
import os
import socket
import signal
import time
import Queue

from chromite.lib import cros_logging as logging
from chromite.lib import metrics
from chromite.lib import parallel

try:
  from infra_libs.ts_mon import config
  import googleapiclient.discovery
except (ImportError, RuntimeError) as e:
  config = None
  logging.warning('Failed to import ts_mon, monitoring is disabled: %s', e)


_WasSetup = False

FLUSH_INTERVAL = 60


@contextlib.contextmanager
def TrivialContextManager():
  """Context manager with no side effects."""
  yield


def SetupTsMonGlobalState(service_name,
                          short_lived=False,
                          indirect=False,
                          auto_flush=True):
  """Uses a dummy argument parser to get the default behavior from ts-mon.

  Args:
    service_name: The name of the task we are sending metrics from.
    short_lived: Whether this process is short-lived and should use the autogen
                 hostname prefix.
    indirect: Whether to create a metrics.METRICS_QUEUE object and a separate
              process for indirect metrics flushing. Useful for forking,
              because forking would normally create a duplicate ts_mon thread.
    auto_flush: Whether to create a thread to automatically flush metrics every
                minute.
  """
  if not config:
    return TrivialContextManager()

  if indirect:
    return _CreateTsMonFlushingProcess([service_name],
                                       {'short_lived': short_lived})

  # google-api-client has too much noisey logging.
  googleapiclient.discovery.logger.setLevel(logging.WARNING)
  parser = argparse.ArgumentParser()
  config.add_argparse_options(parser)
  args = [
      '--ts-mon-target-type', 'task',
      '--ts-mon-task-service-name', service_name,
      '--ts-mon-task-job-name', service_name,
  ]

  # Short lived processes will have autogen: prepended to their hostname and
  # use task-number=PID to trigger shorter retention policies under
  # chrome-infra@, and used by a Monarch precomputation to group across the
  # task number.
  # Furthermore, we assume they manually call ts_mon.flush(), because the
  # ts_mon thread will drop messages if the process exits before it flushes.
  if short_lived:
    auto_flush = False
    fqdn = socket.getfqdn().lower()
    host = fqdn.split('.')[0]
    args.extend(['--ts-mon-task-hostname', 'autogen:' + host,
                 '--ts-mon-task-number', os.getpid()])

  args.extend(['--ts-mon-flush', 'auto' if auto_flush else 'manual'])

  try:
    config.process_argparse_options(parser.parse_args(args=args))
    logging.notice('ts_mon was set up.')
    _WasSetup = True
  except Exception as e:
    logging.warning('Failed to configure ts_mon, monitoring is disabled: %s', e,
                    exc_info=True)


  return TrivialContextManager()


@contextlib.contextmanager
def _CreateTsMonFlushingProcess(setup_args, setup_kwargs):
  """Creates a separate process to flush ts_mon metrics.

  Useful for multiprocessing scenarios where we don't want multiple ts-mon
  threads send contradictory metrics. Instead, functions in
  chromite.lib.metrics will send their calls to a Queue, which is consumed by a
  dedicated flushing process.

  Args:
    setup_args: Arguments sent to SetupTsMonGlobalState in the child process
    setup_kwargs: Keyword arguments sent to SetupTsMonGlobalState in the child
      process

  Side effects:
    Sets chromite.lib.metrics.MESSAGE_QUEUE, which causes the metric functions
    to send their calls to the Queue instead of creating the metrics.
  """
  # If this is nested, we don't need to create another queue and another
  # message consumer. Do nothing to continue to use the existing queue.
  if metrics.MESSAGE_QUEUE:
    return

  with parallel.Manager() as manager:
    message_q = manager.Queue()

    p = multiprocessing.Process(
        target=lambda: _ConsumeMessages(message_q, setup_args, setup_kwargs))
    p.start()

    # this makes the chromite.lib.metric functions use the queue.
    # note - we have to do this *after* forking the ConsumeMessages process.
    metrics.MESSAGE_QUEUE = message_q

    try:
      yield message_q
    finally:
      message_q.put(None)
      logging.info("Waiting for ts_mon flushing process to finish...")
      p.join(timeout=FLUSH_INTERVAL*2)
      if p.is_alive():
        p.terminate()
      if p.exitcode:
        logging.warning("ts_mon_config flushing process did not exit cleanly.")


def _WaitToFlush(last_flush):
  """Sleeps until the next time we can call metrics.flush(), then flushes.

  Args:
    last_flush: timestamp of the last flush
  """
  time_delta = time.time() - last_flush
  time.sleep(max(0, FLUSH_INTERVAL - time_delta))
  metrics.flush()


def _FlushIfReady(pending, last_flush):
  """Call metrics.flush() if we are ready and have pending metrics.

  This allows us to only call flush every FLUSH_INTERVAL seconds.

  Args:
    pending: bool indicating whether there are pending metrics to flush.
    last_flush: time stamp of the last time flush() was called.
  """
  now = time.time()
  time_delta = now - last_flush
  if time_delta > FLUSH_INTERVAL and pending:
    last_flush = now
    time_delta = 0
    metrics.flush()
    pending = False
  else:
    pending = True

  return pending, last_flush, time_delta


def _MethodCallRepr(obj, method, args, kwargs):
  """Gives a string representation of |obj|.|method|(*|args|, **|kwargs|)

  Args:
    obj: An object
    method: A method name
    args: A list of arguments
    kwargs: A dict of keyword arguments
  """
  args_strings = (map(repr, args) +
                  [(str(k) + '=' + repr(v))
                   for (k, v) in kwargs.iteritems()])
  return '%s.%s(%s)' % (repr(obj), method, ', '.join(args_strings))


def _ConsumeMessages(message_q, setup_args, setup_kwargs):
  """Configures ts_mon and gets metrics from a message queue.

  Args:
    message_q: A multiprocessing.Queue to read metrics from.
    setup_args: Arguments to pass to SetupTsMonGlobalState.
    setup_kwargs: Keyword arguments to pass to SetupTsMonGlobalState.
  """
  # Configure ts-mon, but don't start up a sending thread.
  setup_kwargs['auto_flush'] = False
  SetupTsMonGlobalState(*setup_args, **setup_kwargs)

  last_flush = 0
  pending = False

  # If our parent dies, finish flushing before exiting.
  parallel.ExitWithParent(signal.SIGHUP)
  signal.signal(signal.SIGHUP, lambda _sig, _stack: _WaitToFlush(last_flush))

  message = message_q.get()
  while message:
    metric, func, f_args, f_kwargs = message
    try:
      getattr(metric, func)(*f_args, **f_kwargs)
    except Exception:
      logging.exception('Caught an exception while running %s',
                        _MethodCallRepr(metric, func, f_args, f_kwargs))

    pending, last_flush, time_delta = _FlushIfReady(True, last_flush)

    try:
      # Only wait until the next flush time if we have pending metrics.
      timeout = FLUSH_INTERVAL - time_delta if pending else None
      message = message_q.get(timeout=timeout)
    except Queue.Empty:
      # We had pending metrics, but we didn't get a new message. Flush and wait
      # indefinitely.
      pending, last_flush, _ = _FlushIfReady(pending, last_flush)
      # Wait as long as we need to for the next metric.
      message = message_q.get()

  if pending:
    _WaitToFlush(last_flush)
