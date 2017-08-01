# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for chromite.lib.cloud_trace."""
from __future__ import print_function

from chromite.lib import cloud_trace
from chromite.lib import cros_test_lib


class SpanTest(cros_test_lib.MockTestCase):
  """Tests for the Span class."""

  def testToDict(self):
    """Tests that Span instances can be turned into dicts."""
    traceId = 'deadbeefdeadbeefdeadbeefdeadbeef'
    span = cloud_trace.Span(spanId='1234', traceId=traceId, name='Test span')
    self.assertEqual(span.ToDict(), {
        'labels': {},
        'spanId': '1234',
        'name': 'Test span'
    })


class SpanStackTest(cros_test_lib.MockTestCase):
  """Tests for the SpanStack class."""

  def setUp(self):
    self.create_span_mock = self.PatchObject(cloud_trace, 'CreateSpans')

  def testCreateSingleSpan(self):
    """Tests that SpanStack.Span creates a span and sends it."""
    stack = cloud_trace.SpanStack()
    context = stack.Span('foo')
    self.assertEqual(0, self.create_span_mock.call_count)
    with context:
      self.assertEqual(0, self.create_span_mock.call_count)
    self.assertEqual(1, self.create_span_mock.call_count)

  def testCallCreateSpanAtCloseOfStack(self):
    """Test that CreateSpans is called once at the end of a stack."""
    stack = cloud_trace.SpanStack()
    with stack.Span('foo'):
      self.assertEqual(0, self.create_span_mock.call_count)
      with stack.Span('bar'):
        self.assertEqual(0, self.create_span_mock.call_count)
        with stack.Span('zap'):
          self.assertEqual(0, self.create_span_mock.call_count)
      self.assertEqual(0, self.create_span_mock.call_count)
    self.assertEqual(1, self.create_span_mock.call_count)

  def testSpannedDecorator(self):
    """Tests that @stack.Spanned() works."""
    stack = cloud_trace.SpanStack()
    @stack.Spanned('foo')
    def decorated():
      pass

    self.assertEqual(0, self.create_span_mock.call_count)
    decorated()
    self.assertEqual(1, self.create_span_mock.call_count)
