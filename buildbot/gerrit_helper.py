# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing helper class and methods for interacting with Gerrit."""

import constants
import json
import logging
import os

from chromite.buildbot import patch as cros_patch
from chromite.lib import cros_build_lib


class GerritHelper():
  """Helper class to manage interaction with Gerrit server."""
  def __init__(self, internal):
    """Initializes variables for interaction with a gerrit server."""
    if internal:
      ssh_port = constants.GERRIT_INT_PORT
      ssh_host = constants.GERRIT_INT_HOST
    else:
      ssh_port = constants.GERRIT_PORT
      ssh_host = constants.GERRIT_HOST

    self.internal = internal
    self.ssh_prefix = ['ssh', '-p', ssh_port, ssh_host]

  def GetGerritQueryCommand(self, query_list):
    """Returns array corresponding to Gerrit Query command.

    Query is useful for getting information about changelists.  Returns results
    in json format for easy dictionary parsing.  Pass in |query| as the query
    args.
    """
    assert isinstance(query_list, list), 'Query command must be list.'
    return self.ssh_prefix + ['gerrit', 'query', '--format=json'] + query_list

  def GetGerritReviewCommand(self, command_list):
    """Returns array corresponding to Gerrit Review command.

    Review can be used to modify a changelist.  Specifically it can change
    scores, abandon, restore or submit it.  Pass in |command|.
    """
    assert isinstance(command_list, list), 'Review command must be list.'
    return self.ssh_prefix + ['gerrit', 'review'] + command_list


  def GrabChangesReadyForCommit(self, branch):
    """Returns the list of changes to try.

    This methods returns a a list of GerritPatch's to try.
    """
    query_string = ('status:open AND CodeReview=+2 AND Verified=+1 '
                    'AND NOT CodeReview=-2 AND NOT Verified=-1 '
                    'AND branch:%s' % branch)
    # Whitelist specific repositories.
    query_string = (query_string + ' AND '
                    '(project:chromiumos/chromite OR'
                    ' project:chromiumos/platform/crosutils)')
    ready_for_commit = ['--current-patch-set', '"%s"' % query_string]

    query_cmd = self.GetGerritQueryCommand(ready_for_commit)
    raw_results = cros_build_lib.RunCommand(query_cmd,
                                            redirect_stdout=True)

    # Each line returned is a json dict.
    changes_to_commit = []
    for raw_result in raw_results.output.splitlines():
      result_dict = json.loads(raw_result)
      if not 'id' in result_dict:
        logging.debug('No change number found in %s' % result_dict)
        continue

      changes_to_commit.append(cros_patch.GerritPatch(result_dict,
                                                      self.internal))

    return changes_to_commit

  def GrabPatchFromGerrit(self, project, change, commit):
    """Returns the GerritChange described by the arguments.

    Args:
      project: Name of the Gerrit project for the change.
      change:  The change ID for the change.
      commit:  The specific commit hash for the patch from the review.
    """
    grab_change = (
        ['--current-patch-set',
         '"project:%(project)s AND change:%(change)s AND commit:%(commit)s"' %
             {'project': project, 'change': change, 'commit': commit}
        ])

    query_cmd = self.GetGerritQueryCommand(grab_change)
    raw_results = cros_build_lib.RunCommand(query_cmd,
                                            redirect_stdout=True)

    # First line returned is the json.
    raw_result = raw_results.output.splitlines()[0]
    result_dict = json.loads(raw_result)
    assert result_dict.get('id'), 'Code Review json missing change-id!'
    return cros_patch.GerritPatch(result_dict, self.internal)

  def FilterProjectsNotInSourceTree(self, changes, buildroot):
    """Returns new filtered set of relevant changes to this source checkout.

    There may be many miscellaneous reviews in other Gerrit repos that are not
    part of the checked out manifest.  This method returns a filtered list of
    changes with such reviews removed.

    Args:
      changes:  List of GerritPatch objects.
      buildroot:  Buildroot containing manifest to filter against.

    Returns filtered list of GerritPatch objects.
    """
    manifest_path = os.path.join(buildroot, '.repo', 'manifests/full.xml')
    handler = cros_build_lib.ManifestHandler.ParseManifest(manifest_path)
    projects_set = handler.projects.keys()

    changes_to_return = []
    for change in changes:
      if change.project in projects_set:
        changes_to_return.append(change)
      else:
        logging.info('Filtered change %s' % change)

    return changes_to_return
