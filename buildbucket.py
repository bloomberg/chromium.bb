#!/usr/bin/env python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tool for interacting with Buildbucket.

Usage:
  $ depot-tools-auth login https://cr-buildbucket.appspot.com
  $ buildbucket.py \
    put \
    --bucket master.tryserver.chromium.linux \
    --builder my-builder \

  Puts a build into buildbucket for my-builder on tryserver.chromium.linux.
"""

import argparse
import json
import urlparse
import os
import sys

from third_party import httplib2

import auth


BUILDBUCKET_URL = 'https://cr-buildbucket.appspot.com'
PUT_BUILD_URL = urlparse.urljoin(
  BUILDBUCKET_URL,
  '_ah/api/buildbucket/v1/builds',
)


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument(
    '-v',
    '--verbose',
    action='store_true',
  )
  subparsers = parser.add_subparsers(dest='command')
  put_parser = subparsers.add_parser('put')
  put_parser.add_argument(
    '-b',
    '--bucket',
    help=(
      'The bucket to schedule the build on. Typically the master name, e.g.'
      ' master.tryserver.chromium.linux.'
    ),
    required=True,
  )
  put_parser.add_argument(
    '-c',
    '--changes',
    help='A flie to load a JSON list of changes dicts from.',
  )
  put_parser.add_argument(
    '-n',
    '--builder-name',
    help='The builder to schedule the build on.',
    required=True,
  )
  put_parser.add_argument(
    '-p',
    '--properties',
    help='A file to load a JSON dict of properties from.',
  )
  args = parser.parse_args()
  # TODO(smut): When more commands are implemented, refactor this.
  assert args.command == 'put'

  changes = []
  if args.changes:
    try:
      with open(args.changes) as fp:
        changes.extend(json.load(fp))
    except (TypeError, ValueError):
      sys.stderr.write('%s contained invalid JSON list.\n' % args.changes)
      raise

  properties = {}
  if args.properties:
    try:
      with open(args.properties) as fp:
        properties.update(json.load(fp))
    except (TypeError, ValueError):
      sys.stderr.write('%s contained invalid JSON dict.\n' % args.properties)
      raise

  authenticator = auth.get_authenticator_for_host(
    BUILDBUCKET_URL,
    auth.make_auth_config(use_oauth2=True),
  )
  http = authenticator.authorize(httplib2.Http())
  http.force_exception_to_status_code = True
  response, content = http.request(
    PUT_BUILD_URL,
    'PUT',
    body=json.dumps({
      'bucket': args.bucket,
      'parameters_json': json.dumps({
        'builder_name': args.builder_name,
        'changes': changes,
        'properties': properties,
      }),
    }),
    headers={'Content-Type': 'application/json'},
  )

  if args.verbose:
    print content

  return response.status != 200


if __name__ == '__main__':
  sys.exit(main(sys.argv))
