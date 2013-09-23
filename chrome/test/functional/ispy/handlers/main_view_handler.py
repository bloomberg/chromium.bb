# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Request handler to serve the main_view page."""

import jinja2
import json
import os
import re
import sys
import webapp2

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))

import views

from tests.rendering_test_manager import cloud_bucket_impl
from tools import rendering_test_manager

import ispy_auth_constants

JINJA = jinja2.Environment(
    loader=jinja2.FileSystemLoader(os.path.dirname(views.__file__)),
    extensions=['jinja2.ext.autoescape'])


class MainViewHandler(webapp2.RequestHandler):
  """Request handler to serve the main_view page."""

  def get(self):
    """Handles a get request to the main_view page.

    If the batch_name parameter is specified, then a page displaying all of
      the failed runs in the batch will be shown. Otherwise a view listing
      all of the batches available for viewing will be displayed.
    """
    # Get parameters.
    batch_name = self.request.get('batch_name')
    # Set up rendering test manager.
    bucket = cloud_bucket_impl.CloudBucketImpl(
        ispy_auth_constants.KEY, ispy_auth_constants.SECRET,
        ispy_auth_constants.BUCKET)
    manager = rendering_test_manager.RenderingTestManager(bucket)
    # Load the view.
    if batch_name:
      self._GetForBatch(batch_name, manager)
      return
    self._GetAllBatches(manager)

  def _GetAllBatches(self, manager):
    """Renders a list view of all of the batches available in GCS.

    Args:
      manager: An instance of rendering_test_manager.RenderingTestManager.
    """
    template = JINJA.get_template('list_view.html')
    data = {}
    batches = set([re.match(r'^tests/([^/]+)/.+$', path).groups()[0]
                   for path in manager.GetAllPaths('tests/')])
    base_url = '/?batch_name=%s'
    data['links'] = [(batch, base_url % batch) for batch in batches]
    self.response.write(template.render(data))

  def _GetForBatch(self, batch_name, manager):
    """Renders a sorted list of failure-rows for a given batch_name.

    This method will produce a list of failure-rows that are sorted
      in descending order by number of different pixels.

    Args:
      batch_name: The name of the batch to render failure rows from.
      manager: An instance of rendering_test_manager.RenderingTestManager.
    """
    template = JINJA.get_template('main_view.html')
    paths = set([path for path in manager.GetAllPaths('failures/' + batch_name)
                 if path.endswith('actual.png')])
    rows = [self._CreateRow(batch_name, path, manager)
            for path in paths]
    # Function that sorts by the different_pixels field in the failure-info.
    def _Sorter(x, y):
      return cmp(y['info']['different_pixels'],
                 x['info']['different_pixels'])
    self.response.write(template.render({'comparisons': sorted(rows, _Sorter)}))

  def _CreateRow(self, batch_name, path, manager):
    """Creates one failure-row.

    This method builds a dictionary with the data necessary to display a
      failure in the main_view html template.

    Args:
      batch_name: The name of the batch the failure is in.
      path: A path to the failure's actual.png file.
      manager: An instance of rendering_test_manager.RenderingTestManager.

    Returns:
      A dictionary with fields necessary to render a failure-row
        in the main_view html template.
    """
    res = {}
    pattern = r'^failures/%s/([^/]+)/.+$' % batch_name
    res['test_name'] = re.match(
        pattern, path).groups()[0]
    res['batch_name'] = batch_name
    res['info'] = json.loads(manager.cloud_bucket.DownloadFile(
        '/failures/%s/%s/info.txt' % (res['batch_name'], res['test_name'])))
    expected = 'tests/%s/%s/expected.png' % (batch_name, res['test_name'])
    diff = 'failures/%s/%s/diff.png' % (batch_name, res['test_name'])
    res['expected_path'] = expected
    res['diff_path'] = diff
    res['actual_path'] = path
    res['expected'] = manager.cloud_bucket.GetURL(expected)
    res['diff'] = manager.cloud_bucket.GetURL(diff)
    res['actual'] = manager.cloud_bucket.GetURL(path)
    return res

