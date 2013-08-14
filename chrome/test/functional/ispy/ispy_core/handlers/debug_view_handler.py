# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Request handler to display the debug view for a Failure."""

import jinja2
import os
import sys
import webapp2

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))
import views

JINJA = jinja2.Environment(
    loader=jinja2.FileSystemLoader(os.path.dirname(views.__file__)),
    extensions=['jinja2.ext.autoescape'])


class DebugViewHandler(webapp2.RequestHandler):
  """Request handler to display the debug view for a failure."""

  def get(self):
    """Handles get requests to the /debug_view page.

    GET Parameters:
      diff: The path to the diff image on a failure.
      expected: The path to the expected image on a failure.
      actual: The path to the actual image on a failure.
    """
    diff_path = self.request.get('diff')
    expected_path = self.request.get('expected')
    actual_path = self.request.get('actual')
    data = {}

    def _Encode(url):
      return '/image?file_path=%s' % url

    data['diff'] = Encode(diff_path)
    data['expected'] = _Encode(expected_path)
    data['actual'] = _Encode(actual_path)
    template = JINJA.get_template('debug_view.html')
    self.response.write(template.render(data))
