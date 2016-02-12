# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from telemetry.value import histogram_util
from telemetry.value import scalar

from metrics import Metric

HISTOGRAMS_TO_RECORD = [
  {
      'name': 'MediaRouter.Ui.Dialog.LoadedWithData', 'units': 'ms',
      'display_name': 'dialog_loaded_with_data',
      'type': histogram_util.BROWSER_HISTOGRAM,
      'description': 'The latency to render the media router dialog with data.',
  },
  {
      'name': 'MediaRouter.Ui.Dialog.Paint', 'units': 'ms',
      'display_name': 'dialog_paint',
      'type': histogram_util.BROWSER_HISTOGRAM,
      'description': 'The latency to paint the media router dialog.',
  }]


class MediaRouterMetric(Metric):
  "A metric for media router dialog latency from histograms."

  def Start(self, page, tab):
    raise NotImplementedError()

  def Stop(self, page, tab):
    raise NotImplementedError()

  def AddResults(self, tab, results):
    for h in HISTOGRAMS_TO_RECORD:
      result = json.loads(histogram_util.GetHistogram(
          h['type'], h['name'], tab))
      if 'sum' in result:
        # For all the histograms logged here, there's a single entry so sum
        # is the exact value for that entry.
        results.AddValue(scalar.ScalarValue(
            results.current_page, h['display_name'], h['units'],
            result['sum']))
