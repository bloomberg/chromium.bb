# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A library for emitting traces and spans to Google Cloud trace."""
from __future__ import print_function

import contextlib
import functools
import os
import random

from chromite.lib import structured

from googleapiclient import discovery
from oauth2client.client import GoogleCredentials
import google.protobuf.internal.well_known_types as types


CREDS_PATH = '/creds/service_accounts/service-account-trace.json'
SCOPES = ['https://www.googleapis.com/auth/trace.append']
PROJECT_ID = 'google.com:chromeos-infra-logging'


def MakeCreds():
  """Creates a GoogleCredentials object with the trace.append scope."""
  return GoogleCredentials.from_stream(
      os.path.expanduser(CREDS_PATH)
  ).create_scoped(SCOPES)


def Client():
  """Returns a Cloud Trace API client object."""
  return discovery.build('cloudtrace', 'v1', credentials=MakeCreds())


def CreateSpans(traceId, spans, client=None):
  """Creates spans with a call to the /patchTraces endpoint.

  See endpoint documentation here:
  https://cloud.google.com/trace/docs/reference/v1/rest/v1/projects/patchTraces

  Args:
    traceId: A 32-byte hex string, which is the id of the trace containing the
        spans.
    spans: A list of dictionaries representing a TraceSpan. See the doc:
        https://cloud.google.com/trace/docs/reference/v1/rest/v1/projects.traces#TraceSpan
    client: An optional API client object to use. If None, a new client is
        created.
  """
  client = client or Client()
  return client.projects().patchTraces(projectId=PROJECT_ID, body={
      'traces': [
          {
              'traceId': traceId,
              'projectId': PROJECT_ID,
              'spans': spans
          }
      ]
  })


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
  def __init__(self, traceId=None, parentSpanId=None, labels=None, client=None):
    self.traceId = traceId
    self.spans = []
    self.requests = []
    self.last_span_id = parentSpanId
    self.labels = labels
    self.client = client

  @contextlib.contextmanager
  def Span(self, name, **kwargs):
    """Enter a new Span context contained within the top Span of the stack.

    Args:
      name: The name of the span to enter
      **kwargs: The kwargs to construct the span with.

    Side effect:
      Appends the new span object to |spans|, and yields span while in its
      context. While leaving this context, send all pending span requests to
      the cloud trace API.

    Returns:
      A contextmanager whose __enter__() returns the new Span.
    """
    kwargs.setdefault('traceId', self.traceId)
    kwargs.setdefault('labels', self.labels)
    kwargs.setdefault('parentSpanId', self.last_span_id)

    span = Span(name, **kwargs)
    if self.traceId is None:
      self.traceId = span.traceId

    old_span_id, self.last_span_id = self.last_span_id, span.spanId
    self.spans.append(span)

    with span:
      yield span

    self.requests.append(span.ToDict())
    self.spans.pop()
    self.last_span_id = old_span_id

    # TODO(phobbs) this is quite slow. Consider sending spans in a thread,
    # or logging to a file to be sent by a separate process.
    # On the last pop, send a batched request to the API.
    if not self.spans:
      self.SendSpans()

  def SendSpans(self):
    return CreateSpans(self.traceId, self.requests).execute()

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
