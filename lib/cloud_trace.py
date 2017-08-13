# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A library for emitting traces and spans to Google Cloud trace."""
from __future__ import print_function

import contextlib
import errno
import functools
import json
import os
import random

import google.protobuf.internal.well_known_types as types

from chromite.lib import cros_logging as log
from chromite.lib import structured

PROJECT_ID = 'google.com:chromeos-infra-logging'
SPANS_LOG = '/var/log/trace/{pid}-{span_id}.json'

#--- Code for logging spans to a file for later processing. --------------------
def GetSpanLogFilePath(span):
  """Gets the path to write a span to.

  Args:
    span: The span to write.
  """
  return SPANS_LOG.format(pid=os.getpid(), span_id=span.spanId)


def LogSpan(span):
  """Serializes and logs a Span to a file.

  Args:
    span: A Span instance to serialize.
  """
  try:
    with open(GetSpanLogFilePath(span), 'w') as fh:
      fh.write(json.dumps(span.ToDict()))
  # Catch various configuration errors
  except OSError as error:
    if error.errno == errno.EPERM:
      log.warning(
          'Received permissions error while trying to open the span log file.')
      return None
    elif error.errno == errno.ENOENT:
      log.warning('/var/log/traces does not exist; skipping trace log.')
      return None
    else:
      raise

#-- User-facing API ------------------------------------------------------------
class Span(structured.Structured):
  """An object corresponding to a cloud trace Span."""

  VISIBLE_KEYS = (
      'name', 'spanId', 'parentSpanId', 'labels',
      'startTime', 'endTime', 'status')

  def __init__(self, name, spanId=None, parentSpanId=None, labels=None,
               traceId=None):
    """Creates a Span object.

    Args:
      name: The name of the span
      spanId: (optional) A 64-bit number as a string. If not provided, it will
          be generated randomly with .GenerateSpanId().
      parentSpanId: (optional) The spanId of the parent.
      labels: (optional) a dict<string, string> of key/values
      traceId: (optional) A 32 hex digit string referring to the trace
          containing this span. If not provided, a new trace will be created
          with a random id.
    """
    # visible attributes
    self.name = name
    self.spanId = self.GenerateSpanId() if spanId is None else spanId
    self.parentSpanId = parentSpanId
    self.labels = labels or {}
    self.startTime = None
    self.endTime = None

    self.traceId = traceId or self.GenerateTraceId()

  def GenerateSpanId(self):
    """Returns a random 64-bit number as a string."""
    return str(random.randint(0, 2**64))

  def GenerateTraceId(self):
    """Returns a random 128-bit number as a 32-byte hex string."""
    id_number = random.randint(0, 2**128)
    return '%0.32X' % id_number

  def __enter__(self):
    """Enters the span context.

    Side effect: Records the start time as a Timestamp.
    """
    start = types.Timestamp()
    start.GetCurrentTime()
    self.startTime = start.ToJsonString()
    return self

  def __exit__(self, _type, _value, _traceback):
    """Exits the span context.

    Side-effect:
      Record the end Timestamp.
    """
    end = types.Timestamp()
    end.GetCurrentTime()
    self.endTime = end.ToJsonString()


class SpanStack(object):
  """A stack of Span contexts."""
  def __init__(self, traceId=None, parentSpanId=None, labels=None):
    self.traceId = traceId
    self.spans = []
    self.last_span_id = parentSpanId
    self.labels = labels

  def _CreateSpan(self, name, **kwargs):
    """Creates a span instance, setting certain defaults.

    Args:
      name: The name of the span
      **kwargs: The keyword arguments to configure the span with.
    """
    kwargs.setdefault('traceId', self.traceId)
    kwargs.setdefault('labels', self.labels)
    kwargs.setdefault('parentSpanId', self.last_span_id)
    span = Span(name, **kwargs)
    if self.traceId is None:
      self.traceId = span.traceId
    return span

  @contextlib.contextmanager
  def Span(self, name, **kwargs):
    """Enter a new Span context contained within the top Span of the stack.

    Args:
      name: The name of the span to enter
      **kwargs: The kwargs to construct the span with.

    Side effect:
      Appends the new span object to |spans|, and yields span while in its
      context. Pops the span object when exiting the context.

    Returns:
      A contextmanager whose __enter__() returns the new Span.
    """
    span = self._CreateSpan(name, **kwargs)
    old_span_id, self.last_span_id = self.last_span_id, span.spanId
    self.spans.append(span)

    with span:
      yield span

    self.spans.pop()
    self.last_span_id = old_span_id

    # Log each span to a file for later processing.
    LogSpan(span)

  # pylint: disable=docstring-misnamed-args
  def Spanned(self, *span_args, **span_kwargs):
    """A decorator equivalent of 'with span_stack.Span(...)'

    Args:
      *span_args: *args to use with the .Span
      **span_kwargs: **kwargs to use with the .Span

    Returns:
      A decorator to wrap the body of a function in a span block.
    """
    def SpannedDecorator(f):
      """Wraps the body of |f| with a .Span block."""
      @functools.wraps(f)
      def inner(*args, **kwargs):
        with self.Span(*span_args, **span_kwargs):
          f(*args, **kwargs)
      return inner
    return SpannedDecorator
