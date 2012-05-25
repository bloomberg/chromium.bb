#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import urlfetch

from google.appengine.ext import webapp
from google.appengine.ext.webapp.util import run_wsgi_app
from branch_utility import BranchUtility
from resource_fetcher import SubversionFetcher

EXTENSIONS_PATH = 'src/chrome/common/extensions/docs/'


class MainPage(webapp.RequestHandler):
  def get(self):
    path = self.request.path.replace('/chrome/extensions/', '')

    parts = path.split('/')
    if len(parts) > 1:
      filename = parts[1]
    else:
      filename = parts[0]

    if len(path) > 0 and path[0] == '/':
      path = path.strip('/')

    fetcher = SubversionFetcher(urlfetch)
    b_util = BranchUtility(urlfetch)
    channel_name = b_util.GetChannelNameFromPath(path)
    branch = b_util.GetBranchNumberForChannelName(channel_name)

    logging.info(channel_name + ' ' + branch)
    try:
      result = fetcher.FetchResource(branch, EXTENSIONS_PATH + filename)
      content = result.content
      for key in result.headers:
        self.response.headers[key] = result.headers[key]
    except:
      content = 'File not found.'

    self.response.out.write(content)

application = webapp.WSGIApplication([
  ('/.*', MainPage),
], debug=False)


def main():
  run_wsgi_app(application)


if __name__ == '__main__':
  main()
