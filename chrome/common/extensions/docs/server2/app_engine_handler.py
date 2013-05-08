# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from appengine_wrappers import webapp
from handler import Handler
from servlet import Request

class AppEngineHandler(webapp.RequestHandler):
  '''Top-level handler for AppEngine requests. Just converts them into our
  internal Servlet architecture.
  '''
  def get(self):
    request = Request(self.request.path,
                      self.request.url[:-len(self.request.path)],
                      self.request.headers)
    response = Handler(request).Get()
    self.response.out.write(response.content.ToString())
    self.response.headers.update(response.headers)
    self.response.status = response.status
