# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper functions for accessing the issue tracker in a pythonic way."""

from __future__ import print_function

import pprint
import sys

# pylint: disable=F0401
import gdata.client
import gdata.projecthosting.client
# pylint: enable=F0401


DEFAULT_TRACKER_SOURCE = "chromite-tracker-access-1.0"
VERBOSE = True  # Set to True to get extra debug info...


class TrackerAccess(object):
  """Class for accessing the tracker on code.google.com."""

  def __init__(self, email="", password="",
               tracker_source=DEFAULT_TRACKER_SOURCE):
    """TrackerAccess constructor.

    Args:
      email: The email address to Login with; may be "" for anonymous access.
      password: The password that goes with the email address; may be "" if
                the email is "".
      tracker_source: A string describing this program.  This can be anything
                      you like but should should give some indication of which
                      app is making the request.
    """
    # Save parameters...
    self._email = email
    self._password = password
    self._tracker_source = tracker_source

    # This will be initted on first login...
    self._tracker_client = None

  def Login(self):
    """Login, if needed.  This may be safely called more than once.

    Commands will call this function as their first line, so the client
    of this class need not call it themselves unless trying to debug login
    problems.

    This function should be called even if we're accessing anonymously.
    """
    # Bail immediately if we've already logged in...
    if self._tracker_client is not None:
      return

    self._tracker_client = gdata.projecthosting.client.ProjectHostingClient()
    if self._email and self._password:
      self._tracker_client.client_login(self._email, self._password,
                                        source=self._tracker_source,
                                        service="code", account_type='GOOGLE')

  def GetKeyedLabels(self, project_name, issue_id):
    """Get labels of the form "Key-Value" attached to the given issue.

    Any labels that don't have a dash in them are ignored.

    Args:
      project_name: The tracker project to query.
      issue_id: The ID of the issue to query; should be an int but a string
          will probably work too.

    Returns:
      A dictionary mapping key/value pairs from the issue's labels, like:

      {'Area': 'Build',
       'Iteration': '15',
       'Mstone': 'R9.x',
       'Pri': '1',
       'Type': 'Bug'}
    """
    # Login if needed...
    self.Login()

    # Construct the query...
    query = gdata.projecthosting.client.Query(issue_id=issue_id)
    try:
      feed = self._tracker_client.get_issues(project_name, query=query)
    except gdata.client.RequestError as e:
      if VERBOSE:
        print("ERROR: Unable to access bug %s:%s: %s" %
              (project_name, issue_id, str(e)), file=sys.stderr)
      return {}

    # There should be exactly one result...
    assert len(feed.entry) == 1, "Expected exactly 1 result"
    (entry,) = feed.entry

    # We only care about labels that look like: Key-Value
    # We'll return a dictionary of those.
    keyed_labels = {}
    for label in entry.label:
      if "-" in label.text:
        label_key, label_val = label.text.split("-", 1)
        keyed_labels[label_key] = label_val

    return keyed_labels


def _TestGetKeyedLabels(project_name, email, passwordFile, *args):
  """Test code for GetKeyedLabels().

  Args:
    project_name: The name of the project we're looking at.
    email: The email address to use to login.  May be ""
    passwordFile: A file containing the password for the email address.
                  May be "" if email is "" for anon access.
    args: A list of bug IDs to query.
  """
  bug_ids = args
  # If password was specified as a file, read it.
  if passwordFile:
    password = open(passwordFile, "r").read().strip()
  else:
    password = ""

  ta = TrackerAccess(email, password)

  if not bug_ids:
    print("No bugs were specified")
  else:
    for bug_id in bug_ids:
      print(bug_id, ta.GetKeyedLabels(project_name, int(bug_id)))


def _DoHelp(commands, *args):
  """Print help for the script."""

  if len(args) >= 2 and args[0] == "help" and args[1] in commands:
    # If called with arguments 'help' and 'command', show that commands's doc.
    command_name = args[1]
    print(commands[command_name].__doc__)
  else:
    # Something else: show generic help...
    print(
        "Usage %s <command> <command args>\n"
        "\n"
        "Known commands: \n"
        "  %s\n"
        % (sys.argv[0], pprint.pformat(["help"] + sorted(commands))))


def main():
  """Main function of the script."""

  commands = {
      "TestGetKeyedLabels": _TestGetKeyedLabels,
  }

  if len(sys.argv) <= 1 or sys.argv[1] not in commands:
    # Argument 1 isn't in list of commands; show help and pass all arguments...
    _DoHelp(commands, *sys.argv[1:])
  else:
    command_name = sys.argv[1]
    commands[command_name](*sys.argv[2:])
