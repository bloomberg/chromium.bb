#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

# Add the original server location to sys.path so we are able to import
# modules from there.
SERVER_PATH = 'chrome/common/extensions/docs/server2/'
if os.path.abspath(SERVER_PATH) not in sys.path:
  sys.path.append(os.path.abspath(SERVER_PATH))

import logging
import urlfetch

from google.appengine.ext import webapp
from google.appengine.ext.webapp.util import run_wsgi_app

from branch_utility import BranchUtility
from local_fetcher import LocalFetcher
from subversion_fetcher import SubversionFetcher
from template_fetcher import TemplateFetcher
from template_utility import TemplateUtility

EXTENSIONS_PATH = 'chrome/common/extensions/'
DOCS_PATH = 'docs/'
STATIC_PATH = DOCS_PATH + 'static/'
PUBLIC_TEMPLATE_PATH = DOCS_PATH + 'template2/public/'
PRIVATE_TEMPLATE_PATH = DOCS_PATH + 'template2/private/'

class MainPage(webapp.RequestHandler):
  def _FetchResource(self, fetcher, branch, path):
    result = fetcher.FetchResource(branch, path)
    for key in result.headers:
      self.response.headers[key] = result.headers[key]
    return result.content

  def get(self):
    path = self.request.path
    path = path.replace('/chrome/extensions/', '')
    parts = path.split('/')
    filename = parts[-1]

    if len(path) > 0 and path[0] == '/':
      path = path.strip('/')

    branch_util = BranchUtility(urlfetch)
    channel_name = branch_util.GetChannelNameFromPath(path)
    branch = branch_util.GetBranchNumberForChannelName(channel_name)

    if channel_name == 'local':
      fetcher = LocalFetcher(EXTENSIONS_PATH)
    else:
      fetcher = SubversionFetcher(EXTENSIONS_PATH, urlfetch)
    template_fetcher = TemplateFetcher(branch, fetcher)
    template_util = TemplateUtility()

    template = template_fetcher[PUBLIC_TEMPLATE_PATH + filename]
    if template:
      content = template_util.RenderTemplate(template, '{"test": "Hello"}')
    else:
      logging.info('Template for %s not found.' % filename)

      try:
        content = self._FetchResource(fetcher, branch, STATIC_PATH + filename)
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
