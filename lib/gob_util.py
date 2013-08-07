# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Utilities for requesting information for a gerrit server via https.

https://gerrit-review.googlesource.com/Documentation/rest-api.html
"""

import base64
import httplib
import json
import logging
import netrc
import os
from cStringIO import StringIO

try:
  NETRC = netrc.netrc()
except (IOError, netrc.NetrcParseError):
  NETRC = netrc.netrc(os.devnull)
LOGGER = logging.getLogger()


class GOBError(Exception):
  """Exception class for errors commuicating with the gerrit-on-borg service."""
  def __init__(self, http_status, *args, **kwargs):
    super(GOBError, self).__init__(*args, **kwargs)
    self.http_status = http_status


def _QueryString(param_dict, first_param=None):
  """Encodes query parameters in the key:val[+key:val...] format specified here:

  https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#list-changes
  """
  q = [first_param] if first_param else []
  q.extend(['%s:%s' % (key, val) for key, val in param_dict.iteritems()])
  return '+'.join(q)


def CreateHttpConn(host, path, reqtype='GET', headers=None, body=None):
  """Opens an https connection to a gerrit service, and sends a request."""
  conn = httplib.HTTPSConnection(host)
  headers = headers or {}
  auth = NETRC.authenticators(host)
  if auth:
    headers.setdefault('Authorization', 'Basic %s' % (
        base64.b64encode('%s:%s' % (auth[0], auth[2]))))
  if body:
    body = json.JSONEncoder().encode(body)
    headers.setdefault('Content-Type', 'application/json')
  if LOGGER.isEnabledFor(logging.DEBUG):
    LOGGER.debug('%s https://%s/a/%s' % (reqtype, host, path))
    for key, val in headers.iteritems():
      LOGGER.debug('%s: %s' % (key, val))
    if body:
      LOGGER.debug(body)
  conn.request(reqtype, '/a/%s' % path, body=body, headers=headers)
  return conn


def ReadHttpResponse(conn, ignore_404=True):
  """Reads an http response from the argument connection into a string buffer."""
  response = conn.getresponse()
  if ignore_404 and response.status == 404:
    return StringIO()
  if response.status != 200:
    raise GOBError(response.status, response.reason)
  return StringIO(response.read())


def ReadHttpJsonResponse(conn, ignore_404=True):
  """Parses an https response as json."""
  fh = ReadHttpResponse(conn, ignore_404=ignore_404)
  # The first line of the response should always be: )]}'
  s = fh.readline()
  if s and s.rstrip() != ")]}'":
    raise GOBError(200, 'Unexpected json output: %s' % s)
  s = fh.read()
  if not s:
    return None
  return json.loads(s)


def QueryChanges(host, param_dict, first_param=None, limit=None, o_params=None):
  """
  Queries a gerrit-on-borg server for changes matching query terms.

  Args:
    param_dict: A dictionary of search parameters, as documented here:
        http://gerrit-documentation.googlecode.com/svn/Documentation/2.6/user-search.html
    first_param: A change identifier
    limit: Maximum number of results to return.
    o_params: A list of additional output specifiers, as documented here:
        https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#list-changes
  Returns:
    A list of json-decoded query results.
  """
  # Note that no attempt is made to escape special characters; YMMV.
  if not param_dict and not first_param:
    raise RuntimeError('QueryChanges requires search parameters')
  path = 'changes/?q=%s' % _QueryString(param_dict, first_param)
  if limit:
    path = '%s&n=%d' % (path, limit)
  if o_params:
    path = '%s&%s' % (path, '&'.join(['o=%s' % p for p in o_params]))
  # Don't ignore 404; a query should always return a list, even if it's empty.
  return ReadHttpJsonResponse(CreateHttpConn(host, path), ignore_404=False)


def MultiQueryChanges(host, params_list, limit=None):
  """Initiate a query composed of multiple sets of query parameters."""
  if not params_list:
    raise RuntimeError(
        'MultiQueryChanges requires a list of (param_dict, first_param)')
  q = '&'.join(['q=%s' % _QueryString(x[0], x[1]) for x in params_list])
  path = 'changes/?%s' % q
  if limit:
    path = '%s&n=%d' % (path, limit)
  return ReadHttpJsonResponse(CreateHttpConn(host, path), ignore_404=False)


def GetChangeUrl(host, change):
  """Given a gerrit host name and change id, return an url for the change."""
  return 'https://%s/a/changes/%s' % (host, change)


def GetChange(host, change):
  """Query a gerrit server for information about a single change."""
  path = 'changes/%s' % change
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def GetChangeDetail(host, change):
  """Query a gerrit server for extended information about a single change."""
  path = 'changes/%s/detail' % change
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def GetChangeCurrentRevision(host, change):
  """Get information about the latest revision for a given change."""
  return QueryChanges(host, {}, change, o_params=('CURRENT_REVISION'))


def GetChangeRevisions(host, change):
  """Get information about all revisions associated with a change."""
  return QueryChanges(host, {}, change, o_params=('ALL_REVISIONS'))


def GetChangeReview(host, change, revision=None):
  """Get the current review information for a change."""
  if not revision:
    jmsg = GetChangeRevisions(host, change)
    if not jmsg:
      return None
    elif len(jmsg) > 1:
      raise GOBError(200, 'Multiple changes found for ChangeId %s.' % change)
    revision = jmsg[0]['current_revision']
  path = 'changes/%s/revisions/%s/review'
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def AbandonChange(host, change, msg=''):
  """Abandon a gerrit change."""
  path = 'changes/%s/abandon' % change
  body = {'message': msg}
  conn = CreateHttpConn(host, path, reqtype='POST', body=body)
  return ReadHttpJsonResponse(conn, ignore_404=False)


def GetReviewers(host, change):
  """Get information about all reviewers attached to a change."""
  path = 'changes/%s/reviewers' % change
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def GetReview(host, change, revision):
  """Get review information about a specific revision of a change."""
  path = 'changes/%s/revisions/%s/review' % (change, revision)
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def AddReviewers(host, change, add=None):
  """Add reviewers to a change."""
  if not add:
    return
  if isinstance(add, basestring):
    add = (add,)
  path = 'changes/%s/reviewers' % change
  for r in add:
    body = {'reviewer': r}
    conn = CreateHttpConn(host, path, reqtype='POST', body=body)
    jmsg = ReadHttpJsonResponse(conn, ignore_404=False)
  return jmsg


def RemoveReviewers(host, change, remove=None):
  """Remove reveiewers from a change."""
  if not remove:
    return
  if isinstance(remove, basestring):
    remove = (remove,)
  for r in remove:
    path = 'change/%s/reviewers/%s' % (change, r)
    conn = CreateHttpConn(host, path, reqtype='DELETE')
    response = conn.getresponse()
    if response.status != 204:
      raise GOBError(
          'Unexpectedly received a %d http status while deleting reviewer "%s" '
          'from change %s' % (response.status, r, change))


def ResetReviewLabels(host, change, label, value='0', message=None):
  """Reset the value of a given label for all reviewers on a change."""
  # This is tricky, because we want to work on the "current revision", but
  # there's always the risk that "current revision" will change in between
  # API calls.  So, we check "current revision" at the beginning and end; if
  # it has changed, raise an exception.
  jmsg = GetChangeCurrentRevision(host, change)
  if not jmsg:
    raise GOBError(
        200, 'Could not get review information for change "%s"' % change)
  revision = jmsg[0]['current_revision']
  path = 'changes/%s/revisions/%s/review' % (change, revision)
  message = message or (
      '% label set to %s programmatically by chromite.' % (label, value))
  jmsg = GetReview(host, change, revision)
  if not jmsg:
    raise GOBError(200, 'Could not get review information for revison %s '
                   'of change %s' % (revision, change))
  for review in jmsg.get('labels', {}).get('Commit-Queue', {}).get('all', []):
    if review.get('value', value) != value:
      body = {
          'message': message,
          'labels': {label: value},
          'on_behalf_of': review['_account_id'],
      }
      conn = CreateHttpConn(
          host, path, reqtype='POST', body=body)
      response = ReadHttpJsonResponse(conn)
      if response['labels'][label] != value:
        username = review.get('email', jmsg.get('name', ''))
        raise GOBError(200, 'Unable to set %s label for user "%s"'
                       ' on change %s.' % (label, username, change))
  jmsg = GetChangeCurrentRevision(host, change)
  if not jmsg:
    raise GOBError(
        200, 'Could not get review information for change "%s"' % change)
  elif jmsg[0]['current_revison'] != revision:
    raise GOBError(200, 'While resetting labels on change "%s", '
                   'a new patchset was uploaded.' % change)
