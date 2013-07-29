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
import urllib
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
  q = [urllib.quote(first_param)] if first_param else []
  q.extend(['%s:%s' % (key, val) for key, val in param_dict.iteritems()])
  return '+'.join(q)


def CreateHttpConn(host, path, reqtype='GET', headers=None, body=None):
  """Opens an https connection to a gerrit service, and sends a request."""
  conn = httplib.HTTPSConnection(host)
  headers = headers or {}
  bare_host = host.partition(';')[0]
  auth = NETRC.authenticators(bare_host)
  if auth:
    headers.setdefault('Authorization', 'Basic %s' % (
        base64.b64encode('%s:%s' % (auth[0], auth[2]))))
  if body:
    body = json.JSONEncoder().encode(body)
    headers.setdefault('Content-Type', 'application/json')
  if LOGGER.isEnabledFor(logging.DEBUG):
    LOGGER.debug('%s https://%s/a/%s' % (reqtype, host, path))
    for key, val in headers.iteritems():
      if key == 'Authorization':
        val = 'HIDDEN'
      LOGGER.debug('%s: %s' % (key, val))
    if body:
      LOGGER.debug(body)
  conn.request(reqtype, '/a/%s' % path, body=body, headers=headers)
  return conn


def ReadHttpResponse(conn, ignore_404=True):
  """Reads an http response from a connection into a string buffer."""
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


def MultiQueryChanges(host, param_dict, change_list, limit=None, o_params=None):
  """Initiate a query composed of multiple sets of query parameters."""
  if not change_list:
    raise RuntimeError(
        "MultiQueryChanges requires a list of change numbers/id's")
  q = ['q=%s' % urllib.quote(x) for x in change_list]
  if param_dict:
    q.append(_QueryString(param_dict))
  if limit:
    q.append('n=%d' % limit)
  if o_params:
    q.extend(['o=%s' % p for p in o_params])
  path = 'changes/?%s' % '&'.join(q)
  result = ReadHttpJsonResponse(CreateHttpConn(host, path), ignore_404=False)
  return result if len(change_list) == 1 else sum(result, [])


def GetGerritFetchUrl(host):
  """Given a gerrit host name returns URL of a gerrit instance to fetch from."""
  return 'https://%s/a/' % host


def GetChangePageUrl(host, change_number):
  """Given a gerrit host name and change number, return change page url."""
  return 'https://%s/#/c/%d/' % (host, change_number)


def GetChangeUrl(host, change):
  """Given a gerrit host name and change id, return an url for the change."""
  return 'https://%s/a/changes/%s' % (host, change)


def GetChange(host, change):
  """Query a gerrit server for information about a single change."""
  path = 'changes/%s' % change
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def GetChangeDetail(host, change, o_params=None):
  """Query a gerrit server for extended information about a single change."""
  path = 'changes/%s/detail' % change
  if o_params:
    path += '?%s' % '&'.join(['o=%s' % p for p in o_params])
  return ReadHttpJsonResponse(CreateHttpConn(host, path))


def GetChangeCurrentRevision(host, change):
  """Get information about the latest revision for a given change."""
  return QueryChanges(host, {}, change, o_params=('CURRENT_REVISION',))


def GetChangeRevisions(host, change):
  """Get information about all revisions associated with a change."""
  return QueryChanges(host, {}, change, o_params=('ALL_REVISIONS',))


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


def SubmitChange(host, change, wait_for_merge=True):
  """Submits a gerrit change via Gerrit."""
  path = 'changes/%s/submit' % change
  body = {'wait_for_merge': wait_for_merge}
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
  value = str(value)
  revision = jmsg[0]['current_revision']
  path = 'changes/%s/revisions/%s/review' % (change, revision)
  message = message or (
      '%s label set to %s programmatically by chromite.' % (label, value))
  jmsg = GetReview(host, change, revision)
  if not jmsg:
    raise GOBError(200, 'Could not get review information for revison %s '
                   'of change %s' % (revision, change))
  for review in jmsg.get('labels', {}).get('Commit-Queue', {}).get('all', []):
    if str(review.get('value', value)) != value:
      body = {
          'message': message,
          'labels': {label: value},
          'on_behalf_of': review['_account_id'],
      }
      conn = CreateHttpConn(
          host, path, reqtype='POST', body=body)
      response = ReadHttpJsonResponse(conn)
      if str(response['labels'][label]) != value:
        username = review.get('email', jmsg.get('name', ''))
        raise GOBError(200, 'Unable to set %s label for user "%s"'
                       ' on change %s.' % (label, username, change))
  jmsg = GetChangeCurrentRevision(host, change)
  if not jmsg:
    raise GOBError(
        200, 'Could not get review information for change "%s"' % change)
  elif jmsg[0]['current_revision'] != revision:
    raise GOBError(200, 'While resetting labels on change "%s", '
                   'a new patchset was uploaded.' % change)
