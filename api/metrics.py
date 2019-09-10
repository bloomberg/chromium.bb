# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Metrics for general consumption.

See infra/proto/metrics.proto for a description of the type of record that this
module will be creating.
"""

from __future__ import print_function

from chromite.lib import cros_logging as logging
from chromite.utils import metrics


def deserialize_metrics_log(output_events, prefix=None):
  """Read the current metrics events, adding to output_events.

  This layer facilitates converting between the internal
  chromite.utils.metrics representation of metric events and the
  infra/proto/src/chromiumos/metrics.proto output type.

  Args:
    output_events: A chromiumos.MetricEvent protobuf message.
    prefix: A string to prepend to all metric event names.
  """
  timers = {}

  def make_name(name):
    """Prepend a closed-over prefix to the given name."""
    if prefix:
      return '%s.%s' % (prefix, name)
    else:
      return name

  # Reduce over the input events to append output_events.
  for input_event in metrics.read_metrics_events():
    if input_event.op == metrics.OP_START_TIMER:
      timers[input_event.key] = (input_event.name,
                                 input_event.timestamp_epoch_millis)
    elif input_event.op == metrics.OP_STOP_TIMER:
      # TODO(wbbradley): Drop the None fallback https://crbug.com/1001909.
      timer = timers.pop(input_event.key, None)
      if timer is None:
        logging.error('%s: stop timer recorded, but missing start timer!?',
                      input_event.key)
      if timer:
        assert input_event.name == timer[0]
        output_event = output_events.add()
        output_event.name = make_name(timer[0])
        output_event.timestamp_milliseconds = input_event.timestamp_epoch_millis
        output_event.duration_milliseconds = (
            output_event.timestamp_milliseconds - timer[1])
    elif input_event.op == metrics.OP_NAMED_EVENT:
      output_event = output_events.add()
      output_event.name = make_name(input_event.name)
      output_event.timestamp_milliseconds = input_event.timestamp_epoch_millis
    else:
      raise ValueError('unexpected op "%s" found in metric event: %s' % (
          input_event.op, input_event))

  # This is a sanity-check for unclosed timers.
  # TODO(wbbradley): Turn this back into an assert https://crbug.com/1001909.
  if timers:
    logging.error('excess timer metric data left over: %s', timers)
