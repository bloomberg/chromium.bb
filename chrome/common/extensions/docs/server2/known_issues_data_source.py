# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
import logging
import operator
from StringIO import StringIO

import object_store
from url_constants import OPEN_ISSUES_CSV_URL, CLOSED_ISSUES_CSV_URL

class KnownIssuesDataSource:
  """A data source that fetches Apps issues from code.google.com, and returns a
  list of open and closed issues.
  """
  def __init__(self, object_store, url_fetch):
    self._object_store = object_store
    self._url_fetch = url_fetch

  def GetIssues(self, url):
    cached_data = self._object_store.Get(url, object_store.KNOWN_ISSUES).Get()
    if cached_data is not None:
      return cached_data

    issues = []
    response = self._url_fetch.Fetch(url)
    if response.status_code != 200:
      logging.warning('Fetching %s gave status code %s.' %
          (KNOWN_ISSUES_CSV_URL, response.status_code))
      # Return None so we can do {{^known_issues.open}} in the template.
      return None
    issues_reader = csv.DictReader(StringIO(response.content))

    for issue_dict in issues_reader:
      id = issue_dict.get('ID', '')
      title = issue_dict.get('Summary', '')
      if not id or not title:
        continue
      issues.append({ 'id': id, 'title': title })
    issues = sorted(issues, key=operator.itemgetter('title'))
    self._object_store.Set(url, issues, object_store.KNOWN_ISSUES)
    return issues

  def get(self, key):
    return {
      'open': self.GetIssues(OPEN_ISSUES_CSV_URL),
      'closed': self.GetIssues(CLOSED_ISSUES_CSV_URL)
    }.get(key, None)
