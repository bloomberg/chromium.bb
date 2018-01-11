# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros buildresult: Look up results for a single build."""

from __future__ import print_function

import json

from chromite.cli import command
from chromite.lib import commandline
from chromite.cli.cros import cros_cidbcreds
from chromite.lib import cros_logging as logging



def FetchBuildStatus(db, buildbucket_id=None, cidb_id=None):
  """Fetch the build_status dict for a given build.

  Args:
    db: CIDBConnection.
    buildbucket_id: A buildbucket_id as a str.
    cidb_id: A cidb_id as a str.
  """
  # Look up build details in CIDB.
  if buildbucket_id:
    build_status = db.GetBuildStatusWithBuildbucketId(buildbucket_id)
  elif cidb_id:
    build_status = db.GetBuildStatus(cidb_id)
  else:
    raise ValueError('Must set buildbucket_id or cidb_id.')

  # Exit if the build isn't finished yet.
  FINISHED_STATUSES = ('fail', 'pass', 'missing', 'aborted', 'skipped',
                       'forgiven')
  if build_status['status'] not in FINISHED_STATUSES:
    logging.error('Build not finished. Status: %s', build_status['status'])
    raise SystemExit(2)

  # Find stage information.
  build_status['stages'] = db.GetBuildStages(build_status['id'])

  return build_status


def Report(build_statuses):
  """Generate the stdout description of a given build.

  Args:
    build_statuses: List of build_status dict's from FetchBuildStatus.

  Returns:
    str to display as the final report.
  """
  result = ''

  for build_status in build_statuses:
    result += '\n'.join([
        'cidb_id: %s' % build_status['id'],
        'buildbucket_id: %s' % build_status['buildbucket_id'],
        'status: %s' % build_status['status'],
        'artifacts_url: %s' % None,  # TODO(dgarrett): Populate when available.
        'stages:\n'
    ])
    for stage in build_status['stages']:
      result += '  %s: %s\n' % (stage['name'], stage['status'])
    result += '\n'  # Blank line between builds.

  return result


def ReportJson(build_statuses):
  """Generate the json description of a given build.

  Args:
    build_statuses: List of build_status dict's from FetchBuildStatus.

  Returns:
    str to display as the final report.
  """
  report = {}

  for build_status in build_statuses:
    report[build_status['buildbucket_id']] = {
        'cidb_id': build_status['id'],
        'buildbucket_id': build_status['buildbucket_id'],
        'status': build_status['status'],
        'stages': {s['name']: s['status'] for s in build_status['stages']},
        'artifacts_url': None,  # TODO(dgarrett): Populate when available.
    }

  return json.dumps(report)


@command.CommandDecorator('buildresult')
class BuildResultCommand(command.CliCommand):
  """Script that shows build timing for a build, and it's stages."""

  def __init__(self, options):
    super(BuildResultCommand, self).__init__(options)

  @classmethod
  def AddParser(cls, parser):
    super(cls, BuildResultCommand).AddParser(parser)

    # CIDB access credentials.
    creds_args = parser.add_argument_group()

    creds_args.add_argument('--cred-dir', type='path',
                            metavar='CIDB_CREDENTIALS_DIR',
                            help='Database credentials directory with'
                                 ' certificates and other connection'
                                 ' information. Obtain your credentials'
                                 ' at go/cros-cidb-admin .')

    creds_args.add_argument('--update-cidb-creds', dest='force_update',
                            action='store_true',
                            help='force updating the cidb credentials.')

    # What build do we report on?
    request_group = parser.add_mutually_exclusive_group()

    request_group.add_argument(
        '--buildbucket-id', help='Buildbucket ID of build to look up.')
    request_group.add_argument(
        '--cidb-id', help='CIDB ID of the build to look up.')

    # What kind of report do we generate?
    parser.add_argument('--report', default='standard',
                        choices=['standard', 'json'],
                        help='What format is the output in?')

  def Run(self):
    """Run cros buildresult."""
    self.options.Freeze()

    commandline.RunInsideChroot(self)

    credentials = self.options.cred_dir
    if not credentials:
      credentials = cros_cidbcreds.CheckAndGetCIDBCreds(
          force_update=self.options.force_update)

    # Delay import so sqlalchemy isn't pulled in until we need it.
    from chromite.lib import cidb

    db = cidb.CIDBConnection(credentials)

    # Turn the single build into a list, so we can easily add new fetch
    # methods later that query multiple builds.
    build_statuses = [FetchBuildStatus(
        db, self.options.buildbucket_id, self.options.cidb_id)]

    if self.options.report == 'json':
      report = ReportJson(build_statuses)
    else:
      report = Report(build_statuses)

    print(report)
