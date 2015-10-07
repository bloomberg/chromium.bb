# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for requesting information for a gerrit server via https.

https://gerrit-review.googlesource.com/Documentation/rest-api.html
"""

from __future__ import print_function

import base64
import cookielib
import datetime
import httplib
import json
import netrc
import os
import socket
import sys
import urllib
import urlparse
from cStringIO import StringIO

from chromite.cbuildbot import constants
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import retry_util


try:
  NETRC = netrc.netrc()
except (IOError, netrc.NetrcParseError):
  NETRC = netrc.netrc(os.devnull)
TRY_LIMIT = 10
SLEEP = 0.5

# Controls the transport protocol used to communicate with Gerrit servers using
# git. This is parameterized primarily to enable cros_test_lib.GerritTestCase.
GIT_PROTOCOL = 'https'


class GOBError(Exception):
  """Exception class for errors commuicating with the gerrit-on-borg service."""
  def __init__(self, http_status, *args, **kwargs):
    super(GOBError, self).__init__(*args, **kwargs)
    self.http_status = http_status
    self.message = '(%d) %s' % (self.http_status, self.message)


class InternalGOBError(GOBError):
  """Exception class for GOB errors with status >= 500"""


def _QueryString(param_dict, first_param=None):
  """Encodes query parameters in the key:val[+key:val...] format specified here:

  https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#list-changes
  """
  q = [urllib.quote(first_param)] if first_param else []
  q.extend(['%s:%s' % (key, val) for key, val in param_dict.iteritems()])
  return '+'.join(q)


def GetCookies(host, path, cookie_paths=None):
  """Returns cookies that should be set on a request.

  Used by CreateHttpConn for any requests that do not already specify a Cookie
  header. All requests made by this library are HTTPS.

  Args:
    host: The hostname of the Gerrit service.
    path: The path on the Gerrit service, already including /a/ if applicable.
    cookie_paths: Files to look in for cookies. Defaults to looking in the
      standard places where GoB places cookies.

  Returns:
    A dict of cookie name to value, with no URL encoding applied.
  """
  cookies = {}
  if cookie_paths is None:
    cookie_paths = (constants.GOB_COOKIE_PATH, constants.GITCOOKIES_PATH)
  for cookie_path in cookie_paths:
    if os.path.isfile(cookie_path):
      with open(cookie_path) as f:
        for line in f:
          fields = line.strip().split('\t')
          if line.strip().startswith('#') or len(fields) != 7:
            continue
          domain, xpath, key, value = fields[0], fields[2], fields[5], fields[6]
          if cookielib.domain_match(host, domain) and path.startswith(xpath):
            cookies[key] = value
  return cookies


def CreateHttpConn(host, path, reqtype='GET', headers=None, body=None):
  """Opens an https connection to a gerrit service, and sends a request."""
  headers = headers or {}
  bare_host = host.partition(':')[0]
  auth = NETRC.authenticators(bare_host)
  if auth:
    headers.setdefault('Authorization', 'Basic %s' % (
        base64.b64encode('%s:%s' % (auth[0], auth[2]))))
  else:
    logging.debug('No netrc file found')

  if 'Cookie' not in headers:
    cookies = GetCookies(host, '/a/%s' % path)
    headers['Cookie'] = '; '.join('%s=%s' % (n, v) for n, v in cookies.items())

  if 'User-Agent' not in headers:
    headers['User-Agent'] = ' '.join((
        'chromite.lib.gob_util',
        os.path.basename(sys.argv[0]),
        git.GetGitRepoRevision(os.path.dirname(os.path.realpath(__file__))),
    ))

  if body:
    body = json.JSONEncoder().encode(body)
    headers.setdefault('Content-Type', 'application/json')
  if logging.getLogger().isEnabledFor(logging.DEBUG):
    logging.debug('%s https://%s/a/%s', reqtype, host, path)
    for key, val in headers.iteritems():
      if key.lower() in ('authorization', 'cookie'):
        val = 'HIDDEN'
      logging.debug('%s: %s', key, val)
    if body:
      logging.debug(body)
  conn = httplib.HTTPSConnection(host)
  conn.req_host = host
  conn.req_params = {
      'url': '/a/%s' % path,
      'method': reqtype,
      'headers': headers,
      'body': body,
  }
  conn.request(**conn.req_params)
  return conn


def FetchUrl(host, path, reqtype='GET', headers=None, body=None,
             ignore_204=False, ignore_404=True):
  """Fetches the http response from the specified URL into a string buffer.

  Args:
    host: The hostname of the Gerrit service.
    path: The path on the Gerrit service. This will be prefixed with '/a'
          automatically.
    reqtype: The request type. Can be GET or POST.
    headers: A mapping of extra HTTP headers to pass in with the request.
    body: A string of data to send after the headers are finished.
    ignore_204: for some requests gerrit-on-borg will return 204 to confirm
                proper processing of the request. When processing responses to
                these requests we should expect this status.
    ignore_404: For many requests, gerrit-on-borg will return 404 if the request
                doesn't match the database contents.  In most such cases, we
                want the API to return None rather than raise an Exception.

  Returns:
    A string buffer containing the connection's reply.
  """
  def _FetchUrlHelper():
    err_prefix = 'A transient error occured while querying %s:\n' % (host,)
    try:
      conn = CreateHttpConn(host, path, reqtype=reqtype, headers=headers,
                            body=body)
      response = conn.getresponse()
    except socket.error as ex:
      logging.warning('%s%s', err_prefix, str(ex))
      raise

    # Normal/good responses.
    response_body = response.read()
    if response.status == 204 and ignore_204:
      # This exception is used to confirm expected response status.
      raise GOBError(response.status, response.reason)
    if response.status == 404 and ignore_404:
      return StringIO()
    elif response.status == 200:
      return StringIO(response_body)

    # Bad responses.
    logging.debug('response msg:\n%s', response.msg)
    http_version = 'HTTP/%s' % ('1.1' if response.version == 11 else '1.0')
    msg = ('%s %s %s\n%s %d %s\nResponse body: %r' %
           (reqtype, conn.req_params['url'], http_version,
            http_version, response.status, response.reason,
            response_body))

    # Ones we can retry.
    if response.status >= 500:
      # A status >=500 is assumed to be a possible transient error; retry.
      logging.warning('%s%s', err_prefix, msg)
      raise InternalGOBError(response.status, response.reason)

    # Ones we cannot retry.
    home = os.environ.get('HOME', '~')
    url = 'https://%s/new-password' % host
    if response.status in (302, 303, 307):
      err_prefix = ('Redirect found; missing/bad %s/.netrc credentials or '
                    'permissions (0600)?\n See %s' % (home, url))
    elif response.status in (400,):
      err_prefix = 'Permission error; talk to the admins of the GoB instance'
    elif response.status in (401,):
      err_prefix = ('Authorization error; missing/bad %s/.netrc credentials or '
                    'permissions (0600)?\n See %s' % (home, url))
    elif response.status in (422,):
      err_prefix = ('Bad request body?')

    if response.status >= 400:
      # The 'X-ErrorId' header is set only on >= 400 response code.
      logging.warning('%s\n%s\nX-ErrorId: %s', err_prefix, msg,
                      response.getheader('X-ErrorId'))
    else:
      logging.warning('%s\n%s', err_prefix, msg)

    try:
      logging.warning('conn.sock.getpeername(): %s', conn.sock.getpeername())
    except AttributeError:
      logging.warning('peer name unavailable')
    raise GOBError(response.status, response.reason)

  return retry_util.RetryException((socket.error, InternalGOBError), TRY_LIMIT,
                                   _FetchUrlHelper, sleep=SLEEP)


def FetchUrlJson(*args, **kwargs):
  """Fetch the specified URL and parse it as JSON.

  See FetchUrl for arguments.
  """
  fh = FetchUrl(*args, **kwargs)
  # The first line of the response should always be: )]}'
  s = fh.readline()
  if s and s.rstrip() != ")]}'":
    raise GOBError(200, 'Unexpected json output: %s' % s)
  s = fh.read()
  if not s:
    return None
  return json.loads(s)


def QueryChanges(host, param_dict, first_param=None, limit=None, o_params=None,
                 start=None):
  """Queries a gerrit-on-borg server for changes matching query terms.

  Args:
    host: The Gerrit server hostname.
    param_dict: A dictionary of search parameters, as documented here:
        http://gerrit-documentation.googlecode.com/svn/Documentation/2.6/user-search.html
    first_param: A change identifier
    limit: Maximum number of results to return.
    o_params: A list of additional output specifiers, as documented here:
        https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#list-changes
    start: Offset in the result set to start at.

  Returns:
    A list of json-decoded query results.
  """
  # Note that no attempt is made to escape special characters; YMMV.
  if not param_dict and not first_param:
    raise RuntimeError('QueryChanges requires search parameters')
  path = 'changes/?q=%s' % _QueryString(param_dict, first_param)
  if start:
    path = '%s&S=%d' % (path, start)
  if limit:
    path = '%s&n=%d' % (path, limit)
  if o_params:
    path = '%s&%s' % (path, '&'.join(['o=%s' % p for p in o_params]))
  # Don't ignore 404; a query should always return a list, even if it's empty.
  return FetchUrlJson(host, path, ignore_404=False)


def MultiQueryChanges(host, param_dict, change_list, limit=None, o_params=None,
                      start=None):
  """Initiate a query composed of multiple sets of query parameters."""
  if not change_list:
    raise RuntimeError(
        "MultiQueryChanges requires a list of change numbers/id's")
  q = ['q=%s' % '+OR+'.join([urllib.quote(str(x)) for x in change_list])]
  if param_dict:
    q.append(_QueryString(param_dict))
  if limit:
    q.append('n=%d' % limit)
  if start:
    q.append('S=%s' % start)
  if o_params:
    q.extend(['o=%s' % p for p in o_params])
  path = 'changes/?%s' % '&'.join(q)
  try:
    result = FetchUrlJson(host, path, ignore_404=False)
  except GOBError as e:
    msg = '%s:\n%s' % (e.message, path)
    raise GOBError(e.http_status, msg)
  return result


def GetGerritFetchUrl(host):
  """Given a gerrit host name returns URL of a gerrit instance to fetch from."""
  return 'https://%s/' % host


def GetChangePageUrl(host, change_number):
  """Given a gerrit host name and change number, return change page url."""
  return 'https://%s/#/c/%d/' % (host, change_number)


def _GetChangePath(change):
  """Given a change id, return a path prefix for the change."""
  return 'changes/%s' % str(change).replace('/', '%2F')


def GetChangeUrl(host, change):
  """Given a gerrit host name and change id, return an url for the change."""
  return 'https://%s/a/%s' % (host, _GetChangePath(change))


def GetChange(host, change):
  """Query a gerrit server for information about a single change."""
  return FetchUrlJson(host, _GetChangePath(change))


def GetChangeReview(host, change, revision='current'):
  """Get the current review information for a change."""
  path = '%s/revisions/%s/review' % (_GetChangePath(change), revision)
  return FetchUrlJson(host, path)


def GetChangeCommit(host, change, revision='current'):
  """Get the current review information for a change."""
  path = '%s/revisions/%s/commit' % (_GetChangePath(change), revision)
  return FetchUrlJson(host, path)


def GetChangeCurrentRevision(host, change):
  """Get information about the latest revision for a given change."""
  jmsg = GetChangeReview(host, change)
  if jmsg:
    return jmsg.get('current_revision')


def GetChangeDetail(host, change, o_params=None):
  """Query a gerrit server for extended information about a single change."""
  path = '%s/detail' % _GetChangePath(change)
  if o_params:
    path = '%s?%s' % (path, '&'.join(['o=%s' % p for p in o_params]))
  return FetchUrlJson(host, path)


def GetChangeReviewers(host, change):
  """Get information about all reviewers attached to a change."""
  path = '%s/reviewers' % _GetChangePath(change)
  return FetchUrlJson(host, path)


def AbandonChange(host, change, msg=''):
  """Abandon a gerrit change."""
  path = '%s/abandon' % _GetChangePath(change)
  body = {'message': msg}
  return FetchUrlJson(host, path, reqtype='POST', body=body, ignore_404=False)


def RestoreChange(host, change, msg=''):
  """Restore a previously abandoned change."""
  path = '%s/restore' % _GetChangePath(change)
  body = {'message': msg}
  return FetchUrlJson(host, path, reqtype='POST', body=body, ignore_404=False)


def DeleteDraft(host, change):
  """Delete a gerrit draft change."""
  path = _GetChangePath(change)
  try:
    FetchUrl(host, path, reqtype='DELETE', ignore_204=True, ignore_404=False)
  except GOBError as e:
    # On success, gerrit returns status 204; anything else is an error.
    if e.http_status != 204:
      raise
  else:
    raise GOBError(
        200, 'Unexpectedly received a 200 http status while deleting draft %r'
        % change)


def SubmitChange(host, change, revision='current', wait_for_merge=True):
  """Submits a gerrit change via Gerrit."""
  path = '%s/revisions/%s/submit' % (_GetChangePath(change), revision)
  body = {'wait_for_merge': wait_for_merge}
  return FetchUrlJson(host, path, reqtype='POST', body=body, ignore_404=False)


def CheckChange(host, change, sha1=None):
  """Performs consistency checks on the change, and fixes inconsistencies.

  This is useful for forcing Gerrit to check whether a change has already been
  merged into the git repo. Namely, if |sha1| is provided and the change is in
  'NEW' status, Gerrit will check if a change with that |sha1| is in the repo
  and mark the change as 'MERGED' if it exists.

  Args:
    host: The Gerrit host to interact with.
    change: The Gerrit change ID.
    sha1: An optional hint of the commit's SHA1 in Git.
  """
  path = '%s/check' % (_GetChangePath(change),)
  if sha1:
    body, headers = {'expect_merged_as': sha1}, {}
  else:
    body, headers = {}, {'Content-Length': '0'}

  return FetchUrlJson(host, path, reqtype='POST',
                      body=body, ignore_404=False,
                      headers=headers)


def GetReviewers(host, change):
  """Get information about all reviewers attached to a change."""
  path = '%s/reviewers' % _GetChangePath(change)
  return FetchUrlJson(host, path)


def AddReviewers(host, change, add=None):
  """Add reviewers to a change."""
  if not add:
    return
  if isinstance(add, basestring):
    add = (add,)
  path = '%s/reviewers' % _GetChangePath(change)
  for r in add:
    body = {'reviewer': r}
    jmsg = FetchUrlJson(host, path, reqtype='POST', body=body, ignore_404=False)
  return jmsg


def RemoveReviewers(host, change, remove=None):
  """Remove reveiewers from a change."""
  if not remove:
    return
  if isinstance(remove, basestring):
    remove = (remove,)
  for r in remove:
    path = '%s/reviewers/%s' % (_GetChangePath(change), r)
    try:
      FetchUrl(host, path, reqtype='DELETE', ignore_404=False)
    except GOBError as e:
      # On success, gerrit returns status 204; anything else is an error.
      if e.http_status != 204:
        raise
    else:
      raise GOBError(
          200, 'Unexpectedly received a 200 http status while deleting'
               ' reviewer "%s" from change %s' % (r, change))


def SetReview(host, change, revision='current', msg=None, labels=None,
              notify=None):
  """Set labels and/or add a message to a code review."""
  if not msg and not labels:
    return
  path = '%s/revisions/%s/review' % (_GetChangePath(change), revision)
  body = {}
  if msg:
    body['message'] = msg
  if labels:
    body['labels'] = labels
  if notify:
    body['notify'] = notify
  response = FetchUrlJson(host, path, reqtype='POST', body=body)
  if not response:
    raise GOBError(404, 'CL %s not found in %s' % (change, host))
  if labels:
    for key, val in labels.iteritems():
      if ('labels' not in response or key not in response['labels'] or
          int(response['labels'][key] != int(val))):
        raise GOBError(200, 'Unable to set "%s" label on change %s.' % (
            key, change))


def SetTopic(host, change, topic):
  """Set |topic| for a change. If |topic| is empty, it will be deleted"""
  path = '%s/topic' % _GetChangePath(change)
  body = {'topic': topic}
  return FetchUrlJson(host, path, reqtype='PUT', body=body, ignore_404=False)


def ResetReviewLabels(host, change, label, value='0', revision='current',
                      message=None, notify=None):
  """Reset the value of a given label for all reviewers on a change."""
  # This is tricky when working on the "current" revision, because there's
  # always the risk that the "current" revision will change in between API
  # calls.  So, the code dereferences the "current" revision down to a literal
  # sha1 at the beginning and uses it for all subsequent calls.  As a sanity
  # check, the "current" revision is dereferenced again at the end, and if it
  # differs from the previous "current" revision, an exception is raised.
  current = (revision == 'current')
  jmsg = GetChangeDetail(
      host, change, o_params=['CURRENT_REVISION', 'CURRENT_COMMIT'])
  if current:
    revision = jmsg['current_revision']
  value = str(value)
  path = '%s/revisions/%s/review' % (_GetChangePath(change), revision)
  message = message or (
      '%s label set to %s programmatically by chromite.' % (label, value))
  for review in jmsg.get('labels', {}).get(label, {}).get('all', []):
    if str(review.get('value', value)) != value:
      body = {
          'message': message,
          'labels': {label: value},
          'on_behalf_of': review['_account_id'],
      }
      if notify:
        body['notify'] = notify
      response = FetchUrlJson(host, path, reqtype='POST', body=body)
      if str(response['labels'][label]) != value:
        username = review.get('email', jmsg.get('name', ''))
        raise GOBError(200, 'Unable to set %s label for user "%s"'
                       ' on change %s.' % (label, username, change))
  if current:
    new_revision = GetChangeCurrentRevision(host, change)
    if not new_revision:
      raise GOBError(
          200, 'Could not get review information for change "%s"' % change)
    elif new_revision != revision:
      raise GOBError(200, 'While resetting labels on change "%s", '
                     'a new patchset was uploaded.' % change)


def GetTipOfTrunkRevision(git_url):
  """Returns the current git revision on the master branch."""
  parsed_url = urlparse.urlparse(git_url)
  path = parsed_url[2].rstrip('/') + '/+log/master?n=1&format=JSON'
  j = FetchUrlJson(parsed_url[1], path, ignore_404=False)
  if not j:
    raise GOBError(
        'Could not find revision information from %s' % git_url)
  try:
    return j['log'][0]['commit']
  except (IndexError, KeyError, TypeError):
    msg = ('The json returned by https://%s%s has an unfamiliar structure:\n'
           '%s\n' % (parsed_url[1], path, j))
    raise GOBError(msg)


def GetCommitDate(git_url, commit):
  """Returns the date of a particular git commit.

  The returned object is naive in the sense that it doesn't carry any timezone
  information - you should assume UTC.

  Args:
    git_url: URL for the repository to get the commit date from.
    commit: A git commit identifier (e.g. a sha1).

  Returns:
     A datetime object.
  """
  parsed_url = urlparse.urlparse(git_url)
  path = '%s/+log/%s?n=1&format=JSON' % (parsed_url.path.rstrip('/'), commit)
  j = FetchUrlJson(parsed_url.netloc, path, ignore_404=False)
  if not j:
    raise GOBError(
        'Could not find revision information from %s' % git_url)
  try:
    commit_timestr = j['log'][0]['committer']['time']
  except (IndexError, KeyError, TypeError):
    msg = ('The json returned by https://%s%s has an unfamiliar structure:\n'
           '%s\n' % (parsed_url.netloc, path, j))
    raise GOBError(msg)
  try:
    # We're parsing a string of the form 'Tue Dec 02 17:48:06 2014'.
    return datetime.datetime.strptime(commit_timestr,
                                      constants.GOB_COMMIT_TIME_FORMAT)
  except ValueError:
    raise GOBError('Failed parsing commit time "%s"' % commit_timestr)


def GetAccount(host):
  """Get information about the user account."""
  return FetchUrlJson(host, 'accounts/self')
