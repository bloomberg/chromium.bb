#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_tracing
import pyauto
import tracer


class TracingSmokeTest(pyauto.PyUITest):
  """Test basic functionality of the tracing API."""
  def setUp(self):
    super(TracingSmokeTest, self).setUp()
    self._tracer_factory = tracer.TracerFactory(self)

  def testGetData(self):
    """Check that we can find a CrBrowserMain thread."""
    tracer = self._tracer_factory.Produce()
    tracer.BeginTracing()
    model = tracer.EndTracing()
    self.assertEqual(1, len(model.FindAllThreadsNamed('CrBrowserMain')))

  def testMultipleTraces(self):
    """Check that we can run multiple traces on the same tracer."""
    tracer = self._tracer_factory.Produce()
    tracer.BeginTracing()
    model1 = tracer.EndTracing()
    tracer.BeginTracing()
    model2 = tracer.EndTracing()
    self.assertEqual(1, len(model1.FindAllThreadsNamed('CrBrowserMain')))
    self.assertEqual(1, len(model2.FindAllThreadsNamed('CrBrowserMain')))

  def testMultipleTracers(self):
    """Check that we can run multiple traces with multiple tracers."""
    tracer1 = self._tracer_factory.Produce()
    tracer2 = self._tracer_factory.Produce()
    # Nested calls to beginTracing is untested and probably won't work.
    tracer1.BeginTracing()
    model1 = tracer1.EndTracing()
    tracer2.BeginTracing()
    model2 = tracer2.EndTracing()
    self.assertEqual(1, len(model1.FindAllThreadsNamed('CrBrowserMain')))
    self.assertEqual(1, len(model2.FindAllThreadsNamed('CrBrowserMain')))

  def testModelValidAfterTracer(self):
    """Check that a TimelineModel is valid after its Tracer is gone."""
    tracer = self._tracer_factory.Produce()
    del self._tracer_factory
    tracer.BeginTracing()
    model = tracer.EndTracing()
    del tracer
    self.assertEqual(1, len(model.FindAllThreadsNamed('CrBrowserMain')))


if __name__ == '__main__':
  pyauto_tracing.Main()
