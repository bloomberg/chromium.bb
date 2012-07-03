#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cgi
import csv
import logging
import os
import re
import StringIO

from google.appengine.ext import webapp
from google.appengine.ext.webapp import template
from google.appengine.api import memcache
from google.appengine.api import urlfetch

class Issue(object):
  def __init__(self, id, title):
    self.id = id
    self.title = title

KNOWN_ISSUES_CSV_URL = \
    'http://code.google.com/p/chromium/issues/csv?can=1&q=AppsKnownIssues'
KNOWN_ISSUES_CACHE_KEY = 'known-issues'
KNOWN_ISSUES_CACHE_TIME = 300 # seconds

class Handler(webapp.RequestHandler):
  def get(self):
    open_issues, closed_issues = self.getKnownIssues()

    template_path = os.path.join(
        os.path.dirname(__file__), 'app_known_issues_template.html')
    self.response.out.write(template.render(template_path, {
      'open_issues': open_issues,
      'closed_issues': closed_issues,
    }))
    self.response.headers.add_header('Content-Type', 'text/html')
    self.response.headers.add_header('Access-Control-Allow-Origin', '*')

  def getKnownIssues(self):
    cached_result = memcache.get(KNOWN_ISSUES_CACHE_KEY)
    if cached_result: return cached_result

    logging.info('re-fetching known issues')

    open_issues = []
    closed_issues = []

    response = urlfetch.fetch(KNOWN_ISSUES_CSV_URL, deadline=10)

    if response.status_code != 200:
      return [], []

    issues_reader = csv.DictReader(StringIO.StringIO(response.content))

    for issue_dict in issues_reader:
      is_fixed = issue_dict.get('Status', '') == 'Fixed'
      id = issue_dict.get('ID', '')
      title = issue_dict.get('Summary', '')

      if not id or not title:
        continue

      issue = Issue(id, title)
      if is_fixed:
        closed_issues.append(issue)
      else:
        open_issues.append(issue)

    result = (open_issues, closed_issues)
    memcache.add(KNOWN_ISSUES_CACHE_KEY, result, KNOWN_ISSUES_CACHE_TIME)
    return result
