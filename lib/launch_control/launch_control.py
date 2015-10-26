# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Select an Android build, and download symbols for it."""

from __future__ import print_function

import json
import apiclient

from httplib2 import Http
from apiclient.discovery import build
from oauth2client.client import SignedJwtAssertionCredentials

from chromite.lib import commandline
from chromite.lib import cros_logging as logging


def OpenBuildApiProxy(json_key_file):
  """Open an Android Internal Build API Apiary proxy.

  Will NOT error out if authentication fails until the first real request is
  made.

  Args:
    json_key_file: A Json key file to authenticate with. Retrieved from the
                   Google Developer Console associated with the account to
                   be used for the requests.

  Returns:
    Proxy object used to make requests against the API.
  """
  # Load the private key associated with the Google service account.
  with open(json_key_file) as json_file:
    json_data = json.load(json_file)
    credentials = SignedJwtAssertionCredentials(
        json_data['client_email'],
        json_data['private_key'],
        'https://www.googleapis.com/auth/androidbuild.internal')

  # Open an authorized API proxy.
  # See https://g3doc.corp.google.com/wireless/android/build_tools/
  #         g3doc/public/build_data.md
  http_auth = credentials.authorize(Http())
  return build('androidbuildinternal', 'v2beta1', http=http_auth)


def FindRecentBuildIds(build_api_proxy, branch, target):
  """Fetch a list of successful completed build ids for a given branch/target.

  This roughly matches the contents of the first page of build results on the
  launch control website, except filtered for only successful/completed builds.

  Since builds sometimes complete out of order, new builds can be added to the
  list out of order.

  Args:
    build_api_proxy: Result of a previous call to OpenBuildApiProxy.
    branch: Name of branch to search. Ex. 'git_mnc-dr-ryu-release'
    target: Build target to search. Ex. 'ryu-userdebug'

  Returns:
    List of build_ids as integers.
  """
  result = build_api_proxy.build().list(
      buildType='submitted',
      branch=branch,
      buildAttemptStatus='complete',
      successful=True,
      target=target,
  ).execute()

  # Extract the build_ids, arrange oldest to newest.
  return sorted(int(b['buildId']) for b in result['builds'])


def FetchBuildArtifact(build_api_proxy, build_id, target, resource_id,
                       output_file):
  """Fetch debug symbols associated with a given build.

  Args:
    build_api_proxy: Result of a previous call to OpenBuildApiProxy.
    build_id: id of the build to fetch symbols for.
    target: Build to target fetch symbols for. Ex. 'ryu-userdebug'
    resource_id: Resource id to fetch. Ex. 'ryu-symbols-2282124.zip'
    output_file: Path to where to write out the downloaded artifact.
  """
  # Open the download connection.
  download_req = build_api_proxy.buildartifact().get_media(
      buildId=build_id,
      target=target,
      attemptId='latest',
      resourceId=resource_id)

  # Download the symbols file contents.
  with open(output_file, mode='wb') as fh:
    downloader = apiclient.http.MediaIoBaseDownload(
        fh, download_req, chunksize=20 * 1024 * 1024)
    done = False
    while not done:
      _status, done = downloader.next_chunk()


def main(argv):
  """Command line wrapper for integration testing of the above library.

  This library requires the ability to authenticate to an external service that
  is restricted from the general public. So, allow manual integration testing by
  users that have the necessary credentials.
  """
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('--json-key-file', type='path', required=True,
                      help='Json key file for authenticating to service.')
  parser.add_argument('--symbols-file', type='path', default='symbols.zip',
                      help='Where to write symbols file out.')
  parser.add_argument('--branch', type=str, default='git_mnc-dr-ryu-release',
                      help='Branch to locate build for.')
  parser.add_argument('--target', type=str, default='ryu-userdebug',
                      help='Target to locate build for.')

  opts = parser.parse_args(argv)
  opts.Freeze()

  build_proxy = OpenBuildApiProxy(opts.json_key_file)

  build_ids = FindRecentBuildIds(
      build_proxy,
      branch=opts.branch,
      target=opts.target)

  build_id = build_ids[0]

  # 'ryu-userdebug' -> 'ryu'
  board = opts.target.split('-')[0]
  # E.g. 'ryu-symbols-2282124.zip'
  resource_id = '%s-symbols-%s.zip' % (board, build_id)

  logging.info('Selected buildId: %s', build_id)
  FetchBuildArtifact(build_proxy, build_id, opts.target, resource_id,
                     opts.symbols_file)
