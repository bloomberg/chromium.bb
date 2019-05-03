# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides an endpoint for getting details about a sheriffed bug."""
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import

import json
import re

from dashboard import oauth2_decorator
from dashboard.common import request_handler
from dashboard.services import issue_tracker_service


BUGDROID = 'bugdroid1@chromium.org'
REVIEW_RE = r'(Review-Url|Reviewed-on): (https?:\/\/[\/\.\w\d]+)'


class BugDetailsHandler(request_handler.RequestHandler):
  """Gets details about a sheriffed bug."""

  def post(self):
    """POST is the same as GET for this endpoint."""
    self.get()

  @oauth2_decorator.DECORATOR.oauth_required
  def get(self):
    """Response handler to get details about a specific bug.

    Request parameters:
      bug_id: Bug ID number, as a string
    """
    bug_id = int(self.request.get('bug_id'), 0)
    if bug_id <= 0:
      self.ReportError('Invalid or no bug id specified.')
      return

    http = oauth2_decorator.DECORATOR.http()
    self.response.out.write(json.dumps(GetBugDetails(bug_id, http)))


def GetBugDetails(bug_id, http):
  bug_details = _GetDetailsFromMonorail(bug_id, http)
  bug_details['review_urls'] = _GetLinkedRevisions(
      bug_details['comments'])
  bug_details['bisects'] = []
  return bug_details


def _GetDetailsFromMonorail(bug_id, http):
  issue_tracker = issue_tracker_service.IssueTrackerService(http)
  bug_details = issue_tracker.GetIssue(bug_id)
  if not bug_details:
    return {'error': 'Failed to get bug details from monorail API'}
  bug_details['comments'] = issue_tracker.GetIssueComments(bug_id)
  owner = None
  if bug_details.get('owner'):
    owner = bug_details.get('owner').get('name')
  return {
      'comments': bug_details['comments'],
      'owner': owner,
      'published': bug_details['published'],
      'state': bug_details['state'],
      'status': bug_details['status'],
      'summary': bug_details['summary'],
  }


def _GetLinkedRevisions(comments):
  """Parses the comments for commits linked by bugdroid."""
  review_urls = []
  bugdroid_comments = [c for c in comments if c['author'] == BUGDROID]
  for comment in bugdroid_comments:
    m = re.search(REVIEW_RE, comment['content'])
    if m:
      review_urls.append(m.group(2))
  return review_urls
