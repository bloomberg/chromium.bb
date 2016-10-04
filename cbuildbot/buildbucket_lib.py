# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code releated to buildbucket.

Buildbucket is a generic build queue. A build requester can schedule
a build and wait for a result. A building system, such as Buildbot,
can lease it, build it and report a result back.

For more documents about buildbucket, please refer to:
'https://chromium.googlesource.com/infra/infra/+/master/appengine/
cr-buildbucket/README.md'
"""

from __future__ import print_function

import json
import os
import urllib

from chromite.cbuildbot import constants
from chromite.cbuildbot import topology
from chromite.lib import auth
from chromite.lib import cros_logging as logging
from chromite.lib import retry_util

# Methods
PUT_METHOD = 'PUT'
POST_METHOD = 'POST'
GET_METHOD = 'GET'

# Statuses
STARTED_STATUS = 'STARTED'
SCHEDULED_STATUS = 'SCHEDULED'
COMPLETED_STATUS = 'COMPLETED'

STATUS_LIST = (STARTED_STATUS, SCHEDULED_STATUS, COMPLETED_STATUS)

SEARCH_LIMIT_COUNT = 50

WATERFALL_BUCKET_MAP = {
    constants.WATERFALL_INTERNAL:
        constants.CHROMEOS_BUILDBUCKET_BUCKET,
    constants.WATERFALL_EXTERNAL:
        constants.CHROMIUMOS_BUILDBUCKET_BUCKET,
}

class BuildbucketResponseException(Exception):
  """Exception got from Buildbucket Response."""

class NoBuildbucketBucketFoundException(Exception):
  """Failed to found the corresponding buildbucket bucket."""

class NoBuildBucketHttpException(Exception):
  """No Buildbucket http instance exception."""

def GetServiceAccount(service_account=None):
  """Get service account file.

  Args:
    service_account: service account file path.

  Returns:
    Return service_account path if the file exists; else, return None.
  """
  if service_account and os.path.isfile(service_account):
    logging.info('Get service account %s', service_account)
    return service_account
  return None

def BuildBucketAuth(service_account=None):
  """Build buildbucket http auth.

  Args:
    service_account: service account file path.

  Returns:
    Http instance with 'Authorization' inforamtion.
  """
  return auth.AuthorizedHttp(auth.GetAccessToken,
                             service_account_json=service_account)

def GetHost(testjob):
  """Get buildbucket Server host."""
  host = topology.topology[
      topology.BUILDBUCKET_TEST_HOST_KEY if testjob
      else topology.BUILDBUCKET_HOST_KEY]

  return host

def BuildBucketRequest(http, url, method, body, dryrun):
  """Generic buildbucket request.

  Args:
    http: Http instance.
    url: Buildbucket url to send requests.
    method: Request method.
    body: Body of http request (string object).
    dryrun: Whether a dryrun.

  Returns:
    A dict of response content if the request succeeds; else, None.

  Raises:
    BuildbucketResponseException when response['status'] is invalid.
  """
  if dryrun:
    logging.info('Dryrun mode is on; Would have made a request '
                 'with url %s method %s body:\n%s', url, method, body)
    return

  def try_method():
    response, content = http.request(
        url,
        method,
        body=body,
        headers={'Content-Type': 'application/json'},
    )

    if int(response['status']) // 100 != 2:
      raise BuildbucketResponseException(
          'Got a %s response from buildbucket with url: %s\n'
          'content: %s' % (response['status'], url, content))

    # Return content_dict
    return json.loads(content)

  return retry_util.GenericRetry(lambda _: True, 3, try_method)

def PutBuildBucket(body, http, testjob, dryrun):
  """Send Put request to buildbucket server.

  Args:
    body: Body of http request.
    http: Http instance.
    testjob: Whether to use the test instance of the buildbucket server.
    dryrun: Whether a dryrun.

  Returns:
    A dict of response content if the request succeeds; else, None.
  """
  url = 'https://%(hostname)s/_ah/api/buildbucket/v1/builds' % {
      'hostname': GetHost(testjob)
  }

  return BuildBucketRequest(http, url, PUT_METHOD, body, dryrun)

def GetBuildBucket(buildbucket_id, http, testjob, dryrun):
  """Send Get request to buildbucket server.

  Args:
    buildbucket_id: Build to get status.
    http: Http instance.
    testjob: Whether to use the test instance of the buildbucket server.
    dryrun: Whether a dryrun.

  Returns:
    A dict of response content if the request succeeds; else, None.
  """
  url = 'https://%(hostname)s/_ah/api/buildbucket/v1/builds/%(id)s' % {
      'hostname': GetHost(testjob),
      'id': buildbucket_id
  }

  return BuildBucketRequest(http, url, GET_METHOD, None, dryrun)

def CancelBuildBucket(buildbucket_id, http, testjob, dryrun):
  """Send Cancel request to buildbucket server.

  Args:
    buildbucket_id: build to cancel.
    http: Http instance.
    testjob: Whether to use the test instance of the buildbucket server.
    dryrun: Whether a dryrun.

  Returns:
    A dict of response content if the request succeeds; else, None.
  """
  url = 'https://%(hostname)s/_ah/api/buildbucket/v1/builds/%(id)s/cancel' % {
      'hostname': GetHost(testjob),
      'id': buildbucket_id
  }

  return BuildBucketRequest(http, url, POST_METHOD, '{}', dryrun)

def CancelBatchBuildBucket(buildbucket_ids, http, testjob, dryrun):
  """Send CancelBatch request to buildbucket server.

  Args:
    buildbucket_ids: builds to cancel.
    http: Http instance.
    testjob: Whether to use the test instance of the buildbucket server.
    dryrun: Whether a dryrun.

  Returns:
    A dict of response content if the request succeeds; else, None.
  """
  url = 'https://%(hostname)s/_ah/api/buildbucket/v1/builds/cancel' % {
      'hostname': GetHost(testjob)
  }

  body = json.dumps({'build_ids': buildbucket_ids})

  return BuildBucketRequest(http, url, POST_METHOD, body, dryrun)

def SearchBuildBucket(http, testjob, dryrun,
                      buckets=None, tags=None, status=None):
  """Send Search requests to the Buildbucket server.

  Args:
    http: Http instance.
    testjob: Whether to use the test instance of the buildbucket server.
    dryrun: Whether a dryrun.
    buckets: Search for builds the buckets.
    tags: Search for builds containing all the tags.
    status: Search for builds in this status.

  Returns:
    A dict of response content if the request succeeds; else, None.
  """
  params = []
  if buckets:
    assert isinstance(buckets, list), 'buckets must be a list of string.'
    for bucket in buckets:
      params.append(('bucket', bucket))
  if tags:
    assert isinstance(tags, list), 'tags must be a list of string.'
    for tag in tags:
      params.append(('tag', tag))
  if status:
    if status not in STATUS_LIST:
      raise ValueError('status must be one of %s' % str(STATUS_LIST))
    params.append(('status', status))

  params_str = urllib.urlencode(params)

  url = 'https://%(hostname)s/_ah/api/buildbucket/v1/search?%(params_str)s' % {
      'hostname': GetHost(testjob),
      'params_str': params_str
  }

  return BuildBucketRequest(http, url, GET_METHOD, None, dryrun)

def SearchAllBuilds(http, testjob, dryrun, limit=SEARCH_LIMIT_COUNT,
                    buckets=None, tags=None, status=None):
  """Search all qualified builds.

  Args:
    http: Http instance.
    testjob: Whether to use the test instance of the buildbucket server.
    limit: the limit count of search results.
    dryrun: Whether a dryrun.
    buckets: Search for builds in the buckets.
    tags: Search for builds containing all the tags.
    status: Search for builds in this status.

  Returns:
    List of builds.
  """
  if limit <= 0:
    raise ValueError('limit %s must be greater than 0.')

  all_builds = []
  while True:
    content = SearchBuildBucket(
        http, testjob, dryrun, buckets=buckets, tags=tags, status=status)

    builds = GetBuilds(content)

    if not builds:
      logging.debug('No build found.')
      break
    if len(builds) + len(all_builds) >= limit:
      all_builds.extend(builds[0:limit - len(all_builds)])
      logging.info('Reached the search limit %s', limit)
      break

    all_builds.extend(builds)

    if GetNextCursor(content) is None:
      logging.debug('No next_cursor in the response.')
      break

  return all_builds

def HasError(content):
  return content and content.get('error')

def GetErrorReason(content):
  return (content.get('error').get('reason')
          if HasError(content) else None)

def HasBuild(content):
  return content and content.get('build')

def GetBuildId(content):
  return (content.get('build').get('id')
          if HasBuild(content) else None)

def GetBuilds(content):
  return content.get('builds', []) if content else []

def GetBuildIds(content):
  return [b.get('id') for b in GetBuilds(content)]

def GetBuildStatus(content):
  return (content.get('build').get('status')
          if HasBuild(content) else None)

def GetBuildResult(content):
  return (content.get('build').get('result')
          if HasBuild(content) else None)

def GetResultMap(content):
  """Get a build_id to result map."""
  if content is None or content.get('results') is None:
    return

  build_result_map = {}
  for r in content.get('results'):
    if r.get('build_id') is not None:
      build_id = r.pop('build_id')
      build_result_map[build_id] = r

  return build_result_map

def HasNextCursor(content):
  return content and content.get('next_cursor')

def GetNextCursor(content):
  return (content.get('next_cursor')
          if HasNextCursor(content) else None)
