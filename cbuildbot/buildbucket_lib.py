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

from chromite.cbuildbot import topology
from chromite.lib import auth
from chromite.lib import cros_logging as logging
from chromite.lib import retry_util

# from third_party
import httplib2

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
  return auth.Authorize(auth.GetAccessToken,
                        httplib2.Http(),
                        service_account_json=service_account)

def GetBuildBucketPutUrl(testjob):
  """Get buildbucket Put request url.

  Args:
    testjob: Whether to use the test instance of the buildbucket server.

  Returns:
    Buildbucket Put url.
  """
  host = topology.topology[
      topology.BUILDBUCKET_TEST_HOST_KEY if testjob
      else topology.BUILDBUCKET_HOST_KEY]
  return (
      'https://{hostname}/_ah/api/buildbucket/v1/builds'.format(
          hostname=host))

def PutBuildBucket(body, http, buildbucket_put_url, dryrun):
  """Send Put request to buildbucket server.

  Args:
    body: Body of http request.
    http: Http instance.
    buildbucket_put_url: Buildbucket Put url.
    dryrun: Whether a dryrun.

  Returns:
    buildbucket_id if Put operation succeeds.
  """
  def try_put():
    response, content = http.request(
        buildbucket_put_url,
        'PUT',
        body=body,
        headers={'Content-Type': 'application/json'},
    )

    if int(response['status']) // 100 != 2:
      raise Exception('Got a %s response from Buildbucket.'
                      % response['status'])

    content_dict = json.loads(content)
    # Return buildbucket_id.
    return content_dict['build']['id']

  if dryrun:
    logging.info('dryrun mode is on; skipping request to buildbucket. '
                 'Would have made a request with body:\n%s', body)
    return

  return retry_util.GenericRetry(lambda _: True, 3, try_put)
