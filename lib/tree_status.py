# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Manage tree status."""

from __future__ import print_function

import json
import os
import urllib

from chromite.lib import constants
from chromite.lib import alerts
from chromite.lib import cros_logging as logging


_LUCI_MILO_BUILDBOT_URL = 'https://luci-milo.appspot.com/buildbot'

_LOGDOG_URL = ('https://luci-logdog.appspot.com/v/'
               '?s=chromeos/buildbucket/cr-buildbucket.appspot.com/'
               '%s/%%2B/steps/%s/0/stdout')

# Will redirect:
#   https://ci.chromium.org/b/8914470887449121184
# to:
#   https://ci.chromium.org/p/chromeos/builds/b8914470887449121184
_MILO_BUILD_URL = 'https://ci.chromium.org/b/%(buildbucket_id)s'


def GetGardenerEmailAddresses():
  """Get the email addresses of the gardeners.

  Returns:
    Gardener email addresses.
  """
  try:
    response = urllib.urlopen(constants.CHROME_GARDENER_URL)
    if response.getcode() == 200:
      return json.load(response)['emails']
  except (IOError, ValueError, KeyError) as e:
    logging.error('Could not get gardener emails: %r', e)
  return None


def GetHealthAlertRecipients(builder_run):
  """Returns a list of email addresses of the health alert recipients."""
  recipients = []
  for entry in builder_run.config.health_alert_recipients:
    if '@' in entry:
      # If the entry is an email address, add it to the list.
      recipients.append(entry)
    elif entry == constants.CHROME_GARDENER:
      # Add gardener email address.
      recipients.extend(GetGardenerEmailAddresses())

  return recipients


def SendHealthAlert(builder_run, subject, body, extra_fields=None):
  """Send a health alert.

  Health alerts are only sent for regular buildbots and Pre-CQ buildbots.

  Args:
    builder_run: BuilderRun for the main cbuildbot run.
    subject: The subject of the health alert email.
    body: The body of the health alert email.
    extra_fields: (optional) A dictionary of additional message header fields
                  to be added to the message. Custom field names should begin
                  with the prefix 'X-'.
  """
  if builder_run.InEmailReportingEnvironment():
    server = alerts.GmailServer(
        token_cache_file=constants.GMAIL_TOKEN_CACHE_FILE,
        token_json_file=constants.GMAIL_TOKEN_JSON_FILE)
    alerts.SendEmail(subject,
                     GetHealthAlertRecipients(builder_run),
                     server=server,
                     message=body,
                     extra_fields=extra_fields)


def ConstructMiloBuildURL(buildbucket_id):
  """Return a Milo build URL.

  Args:
    buildbucket_id: Buildbucket id of the build to link.

  Returns:
    The fully formed URL.
  """
  # Only local tryjobs will not have a buildbucket_id but they also do not have
  # a web UI to point at. Generate a fake URL.
  buildbucket_id = buildbucket_id or 'fake_bb_id'
  return _MILO_BUILD_URL % {'buildbucket_id': buildbucket_id}


def ConstructDashboardURL(buildbot_master_name, builder_name, build_number):
  """Return the dashboard (luci-milo) URL for this run

  Args:
    buildbot_master_name: Name of buildbot master, e.g. chromeos
    builder_name: Builder name on buildbot dashboard.
    build_number: Build number for this validation attempt.

  Returns:
    The fully formed URL.
  """
  url_suffix = '%s/%s' % (builder_name, str(build_number))
  url_suffix = urllib.quote(url_suffix)
  return os.path.join(
      _LUCI_MILO_BUILDBOT_URL, buildbot_master_name, url_suffix)

def ConstructLogDogURL(build_number, stage):
  return _LOGDOG_URL % (str(build_number), stage)


def ConstructViceroyBuildDetailsURL(build_id):
  """Return the dashboard (viceroy) URL for this run.

  Args:
    build_id: CIDB id for the master build.

  Returns:
    The fully formed URL.
  """
  _link = ('https://viceroy.corp.google.com/'
           'chromeos/build_details?build_id=%(build_id)s')
  return _link % {'build_id': build_id}


def ConstructGoldenEyeSuiteDetailsURL(job_id=None, build_id=None):
  """Return the dashboard (goldeneye) URL of suite details for job or build.

  Args:
    job_id: AFE job id.
    build_id: CIDB id for the master build.

  Returns:
    The fully formed URL.
  """
  if job_id is None and build_id is None:
    return None
  _link = 'http://cros-goldeneye/healthmonitoring/suiteDetails?'
  if job_id:
    return _link + 'suiteId=%d' % int(job_id)
  else:
    return _link + 'cidbBuildId=%d' % int(build_id)


def ConstructGoldenEyeBuildDetailsURL(build_id):
  """Return the dashboard (goldeneye) URL for this run.

  Args:
    build_id: CIDB id for the build.

  Returns:
    The fully formed URL.
  """
  _link = ('http://go/goldeneye/'
           'chromeos/healthmonitoring/buildDetails?id=%(build_id)s')
  return _link % {'build_id': build_id}


def ConstructAnnotatorURL(build_id):
  """Return the build annotator URL for this run.

  Args:
    build_id: CIDB id for the master build.

  Returns:
    The fully formed URL.
  """
  _link = ('https://chromiumos-build-annotator.googleplex.com/'
           'build_annotations/edit_annotations/master-paladin/%(build_id)s/?')
  return _link % {'build_id': build_id}
