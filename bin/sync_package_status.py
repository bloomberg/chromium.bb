#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Synchronize issues in Package Status spreadsheet with Issue Tracker."""

import optparse
import os
import sys
import urllib
import xml.dom.minidom

import gdata.projecthosting.client
import gdata.spreadsheet.service

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
import chromite.lib.cros_build_lib as cros_lib
import chromite.lib.gdata_lib as gdata_lib
import chromite.lib.operation as operation
import chromite.lib.upgrade_table as utable

# pylint: disable=W0201,R0904

PROJECT_NAME = 'chromium-os'

SS_KEY = '0AsXDKtaHikmcdEp1dVN1SG1yRU1xZEw1Yjhka2dCSUE'
PKGS_WS_NAME = 'Packages'

CROS_ORG = 'chromium.org'
CHROMIUMOS_SITE = 'http://www.%s/%s' % (CROS_ORG, PROJECT_NAME)
PKG_UPGRADE_PAGE = '%s/gentoo-package-upgrade-process' % CHROMIUMOS_SITE
DOCS_SITE = 'https://docs.google.com/a'
PKG_STATUS_PAGE = '%s/%s/spreadsheet/ccc?key=%s' % (DOCS_SITE, CROS_ORG, SS_KEY)

COL_PACKAGE = gdata_lib.PrepColNameForSS(utable.UpgradeTable.COL_PACKAGE)
COL_TEAM = gdata_lib.PrepColNameForSS('Team/Lead')
COL_OWNER = gdata_lib.PrepColNameForSS('Owner')
COL_TRACKER = gdata_lib.PrepColNameForSS('Tracker')

ARCHES = ('amd64', 'arm', 'x86')

oper = operation.Operation('sync_package_status')

class SyncError(RuntimeError):
  """Extend RuntimeError for use in this module."""

class IssueComment(object):
  """Represent a Tracker issue comment."""

  __slots__ = ['title', 'text']

  def __init__(self, title, text):
    self.title = title
    self.text = text

  def __str__(self):
    text = '\n  '.join(self.text.split('\n'))
    return '%s:\n  %s' % (self.title, text)

class Issue(object):
  """Represents one Tracker Issue."""

  SlotDefaults = {
    'comments': [], # List of IssueComment objects
    'id': 0,        # Issue id number (int)
    'labels': [],   # List of text labels
    'owner': None,  # Current owner (text, chromium.org account)
    'status': None, # Current issue status (text) (e.g. Assigned)
    'summary': None,# Issue summary (first comment)
    'title': None,  # Title text
    }

  __slots__ = SlotDefaults.keys()

  def __init__(self, tracker_issue=None, **kwargs):
    """Init for one Issue object.

    |tracker_issue| - Optional, Tracker issue object to use to
    initialize attributes.
    |kwargs| - key/value arguments to give initial values to
    any additional attributes on |self|.
    """
    # Start every attribute with a default value.
    for slot in self.__slots__:
      setattr(self, slot, self.SlotDefaults[slot])

    # Initialize from real Tracker issue if given.
    if tracker_issue:
      self.InitFromTracker(tracker_issue)

    # Optional overwrite of any attribute using kwargs.
    for slot in self.__slots__:
      if slot in kwargs:
        setattr(self, slot, kwargs[slot])

  def __str__(self):
    """Pretty print of issue."""
    lines = ['Issue %d - %s' % (self.id, self.title),
             'Status: %s, Owner: %s' % (self.status, self.owner),
             'Labels: %s' % ', '.join(self.labels),
             ]

    if self.summary:
      lines.append('Summary: %s' % self.summary)

    if self.comments:
      for comment in self.comments:
        lines.append('%s' % comment)

    return '\n'.join(lines)

  def InitFromTracker(self, t_issue):
    """Initialize |self| from tracker issue |t_issue|"""

    self.id = int(t_issue.id.text.split('/')[-1])
    self.labels = [label.text for label in t_issue.label]
    if t_issue.owner:
      self.owner = t_issue.owner.username.text
    self.status = t_issue.status.text
    self.summary = t_issue.content.text
    self.title = t_issue.title.text
    self.comments = self.GetTrackerIssueComments(self.id)

  def GetTrackerIssueComments(self, issue_id):
    """Retrieve comments for |issue_id| from comments URL"""
    comments = []

    feeds = 'http://code.google.com/feeds'
    url = '%s/issues/p/%s/issues/%d/comments/full' % (feeds, PROJECT_NAME,
                                                      issue_id)
    doc = xml.dom.minidom.parse(urllib.urlopen(url))
    entries = doc.getElementsByTagName('entry')
    for entry in entries:
      title = entry.getElementsByTagName('title')[0].firstChild.nodeValue
      text = entry.getElementsByTagName('content')[0].firstChild.nodeValue
      comments.append(IssueComment(title, text))

    return comments

class TrackerComm(object):
  """Class to manage communication with Tracker."""

  __slots__ = [
    'creds',       # gdata_lib.Creds object
    'it_client',   # Issue Tracker client
    ]

  def __init__(self, creds):
    self.creds = creds

    self.it_client = gdata.projecthosting.client.ProjectHostingClient()
    self.it_client.source = 'package_status_upgrade'
    self.it_client.ClientLogin(creds.user, creds.password,
                               source=self.it_client.source,
                               service='code',
                               account_type='GOOGLE')

  def GetTrackerIssueById(self, tid):
    """Get tracker issue given |tid| number.  Return Issue object if found."""

    query = gdata.projecthosting.client.Query(issue_id=str(tid))
    feed = self.it_client.get_issues('chromium-os', query=query)

    if feed.entry:
      return Issue(feed.entry[0])
    return None

  def CreateTrackerIssue(self, issue):
    """Create a new issue in Tracker according to |issue|."""
    created = self.it_client.add_issue(project_name=PROJECT_NAME,
                                       title=issue.title,
                                       content=issue.summary,
                                       author=self.creds.user,
                                       status=issue.status,
                                       owner=issue.owner,
                                       labels=issue.labels)
    issue.id = int(created.id.text.split('/')[-1])
    return issue.id

  def AppendTrackerIssueById(self, issue_id, comment):
    """Append |comment| to issue |issue_id| in Tracker"""
    self.it_client.update_issue(project_name=PROJECT_NAME,
                                issue_id=issue_id,
                                author=self.creds.user,
                                comment=comment)
    return issue_id

class SpreadsheetComm(object):
  """Class to manage communication with one Google Spreadsheet worksheet."""

  __slots__ = [
    'columns',     # List of column names
    'creds',       # gdata_lib.Creds object
    'gd_client',   # Google Data client
    'ss_key',      # Spreadsheet key
    'ws_key',      # Worksheet key
    ]

  def __init__(self, creds, ss_key, ws_name):
    self.creds = creds
    self._LoginWithUserPassword(creds.user, creds.password)

    self.ss_key = ss_key
    self.ws_key = self._GetWorksheetKey(ss_key, ws_name)

    self.columns = self._GetColumns()

  def _LoginWithUserPassword(self, user, password):
    """Set up and connect the Google Doc client using email/password."""
    gd_client = gdata_lib.RetrySpreadsheetsService()

    gd_client.source = 'Sync Package Status'
    gd_client.email = user
    gd_client.password = password
    gd_client.ProgrammaticLogin()
    self.gd_client = gd_client

  def _GetWorksheetKey(self, ss_key, ws_name):
    """Get the worksheet key with name |ws_name| in spreadsheet |ss_key|."""
    feed = self.gd_client.GetWorksheetsFeed(ss_key)
    # The worksheet key is the last component in the URL (after last '/')
    for entry in feed.entry:
      if ws_name == entry.title.text:
        return entry.id.text.split('/')[-1]

    oper.Die('Unable to find worksheet "%s" in spreadsheet "%s"' %
             (ws_name, ss_key))

  def _GetColumns(self):
    """Return list of column names in worksheet."""
    columns = []

    query = gdata.spreadsheet.service.CellQuery()
    query['max-row'] = '1'
    feed = self.gd_client.GetCellsFeed(self.ss_key, self.ws_key, query=query)
    for entry in feed.entry:
      columns.append(entry.content.text)

    return columns

  def GetColumnIndex(self, colName):
    """Get the column index (starting at 1) for column |colName|"""
    for ix, col in enumerate(self.columns):
      if colName == col:
        # Spreadsheet column indices start at 1.
        return ix + 1

    return None

  def GetAllRowsAsDicts(self):
    """Get every row in spreadsheet as dicts of key/value pairs."""
    feed = self.gd_client.GetListFeed(self.ss_key, self.ws_key)
    rows = []
    for entry in feed.entry:
      row = {}
      for key, val in entry.custom.items():
        row[key] = gdata_lib.ScrubValFromSS(val.text)

      rows.append(row)

    return rows

  def ReplaceCellValue(self, rowIx, colIx, val):
    """Replace cell value at |rowIx| and |colIx| with |val|"""
    self.gd_client.UpdateCell(rowIx, colIx, val, self.ss_key, self.ws_key)

  def ClearCellValue(self, rowIx, colIx):
    """Clear cell value at |rowIx| and |colIx|"""
    self.ReplaceCellValue(rowIx, colIx, None)

class Syncer(object):
  """Class to manage synchronizing between spreadsheet and Tracker."""

  # Map spreadsheet team names to Tracker team values.
  VALID_TEAMS = {'build': 'BuildRelease',
                 'kernel': 'Kernel',
                 'security': 'Security',
                 'system': 'Systems',
                 'ui': 'UI',
                 }
  UPGRADE_STATES = set([utable.UpgradeTable.STATE_NEEDS_UPGRADE,
                        utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_PATCHED,
                        utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_DUPLICATED,
                        ])

  __slots__ = [
    'owners',         # Set of owners to select (None means no filter)
    'pretend',        # If True, make no real changes
    'scomm',          # SpreadsheetComm
    'tcomm',          # TrackerComm
    'teams',          # Set of teams to select (None means no filter)
    'tracker_col_ix', # Index of Tracker column in spreadsheet
    'verbose',        # Verbose boolean
    ]

  def __init__(self, tcomm, scomm, pretend=False, verbose=False):
    self.tcomm = tcomm
    self.scomm = scomm

    self.tracker_col_ix = scomm.GetColumnIndex('Tracker')

    self.teams = None
    self.owners = None

    self.pretend = pretend
    self.verbose = verbose

  def _ReduceTeamName(self, team):
    """Translate |team| from spreadsheet/commandline name to short name.

    For example:  build/bdavirro --> build, build --> build
    """
    if team:
      return team.lower().split('/')[0]
    return None

  def SetTeamFilter(self, teamarg):
    """Set team filter using colon-separated team names in |teamarg|

    Resulting filter in self.teams is set of "reduced" team names.
    Examples:
      'build:system:ui' --> set(['build', 'system', 'ui'])
      'Build:system:UI' --> set(['build', 'system', 'ui'])

    If an invalid team name is given oper.Die is called with explanation.
    """
    if teamarg:
      teamlist = []
      for team in teamarg.split(':'):
        t = self._ReduceTeamName(team)
        if t in self.VALID_TEAMS:
          teamlist.append(t)
        else:
          oper.Die('Invalid team name "%s".  Choose from: %s' %
                   (team, ','.join(self.VALID_TEAMS.keys())))
      self.teams = set(teamlist)
    else:
      self.teams = None

  def _ReduceOwnerName(self, owner):
    """Translate |owner| from spreadsheet/commandline name to short name.

    For example:  joe@chromium.org -> joe, joe --> joe
    """
    if owner:
      return owner.lower().split('@')[0]
    return None

  def SetOwnerFilter(self, ownerarg):
    """Set owner filter using colon-separated owner names in |ownerarg|"""
    if ownerarg:
      self.owners = set([self._ReduceOwnerName(o) for o in ownerarg.split(':')])
    else:
      self.owners = None

  def _RowPassesFilters(self, row):
    """Return true if |row| passes any team/owner filters"""
    if self.teams:
      team = self._ReduceTeamName(row[COL_TEAM])
      if team not in self.teams:
        return False

    if self.owners:
      owner = self._ReduceOwnerName(row[COL_OWNER])
      if owner not in self.owners:
        return False

    return True

  def Sync(self):
    """Do synchronization between Spreadsheet and Tracker."""

    errors = []

    # Go over each row in Spreadsheet.
    rows = self.scomm.GetAllRowsAsDicts()
    for rowIx, row in enumerate(rows):
      # Spreadsheet row index starts at 1, and we don't count
      # the header row.  So add 2.
      rowIx += 2
      if not self._RowPassesFilters(row):
        oper.Info('\nSkipping row %d, pkg: %r (team=%s, owner=%s) ...' %
                  (rowIx, row[COL_PACKAGE], row[COL_TEAM], row[COL_OWNER]))
        continue

      oper.Info('\nProcessing row %d, pkg: %r (team=%s, owner=%s) ...' %
                (rowIx, row[COL_PACKAGE], row[COL_TEAM], row[COL_OWNER]))

      try:
        new_issue = self._GenIssueForRow(row)
        old_issue_id = self._GetRowTrackerId(row)

        if new_issue and not old_issue_id:
          self._CreateRowIssue(rowIx, row, new_issue)
        elif not new_issue and old_issue_id:
          self._ClearRowIssue(rowIx, row)
        else:
          # Nothing to do for this package.
          reason = 'already has issue' if old_issue_id else 'no upgrade needed'
          oper.Notice('Nothing to do for row %d, package %r: %s.' %
                      (rowIx, row[COL_PACKAGE], reason))
      except SyncError:
        errors.append('Error processing row %d, pkg: %r.  See above.' %
                      (rowIx, row[COL_PACKAGE]))

    if errors:
      raise SyncError('\n'.join(errors))

  def _GetRowValue(self, row, colName, arch=None):
    """Get value from |row| at |colName|, adjusted for |arch|"""
    if arch:
      colName = utable.UpgradeTable.GetColumnName(colName, arch=arch)
    colName = gdata_lib.PrepColNameForSS(colName)
    return row[colName]

  def _GenIssueForRow(self, row):
    """Generate an Issue object for |row| if applicable"""
    # Row needs an issue if it "needs upgrade" on any platform.
    statuses = {}
    needs_issue = False
    for arch in ARCHES:
      state = self._GetRowValue(row, utable.UpgradeTable.COL_STATE, arch)
      statuses[arch] = state
      if state in self.UPGRADE_STATES:
        needs_issue = True

    if not needs_issue:
      return None

    pkg = row[COL_PACKAGE]
    team = self._ReduceTeamName(row[COL_TEAM])
    if not team:
      oper.Error('Unable to create Issue for package "%s" because no '
                 'team value is specified.' % pkg)
      raise SyncError()

    team_label = self.VALID_TEAMS[team]
    labels = ['Type-Task',
              'Area-LinuxUserSpace',
              'Pri-2',
              'Team-%s' % team_label,
              ]

    owner = self._ReduceOwnerName(row[COL_OWNER])
    status = 'Untriaged'
    if owner:
      owner = owner + '@chromium.org'
      status = 'Available'
    else:
      owner = None # Rather than ''

    title = '%s package needs upgrade from upstream Portage' % pkg

    lines = ['The %s package can be upgraded from upstream Portage' % pkg,
             '',
             'At this moment the status on each arch is as follows:',
             ]

    for arch in sorted(statuses):
      arch_status = statuses[arch]
      if arch_status:
        # Get all versions for this arch right now.
        curr_ver_col = utable.UpgradeTable.COL_CURRENT_VER
        curr_ver = self._GetRowValue(row, curr_ver_col, arch)
        stable_upst_ver_col = utable.UpgradeTable.COL_STABLE_UPSTREAM_VER
        stable_upst_ver = self._GetRowValue(row, stable_upst_ver_col, arch)
        latest_upst_ver_col = utable.UpgradeTable.COL_LATEST_UPSTREAM_VER
        latest_upst_ver = self._GetRowValue(row, latest_upst_ver_col, arch)

        arch_vers = ['Current version: %s' % curr_ver,
                     'Stable upstream version: %s' % stable_upst_ver,
                     'Latest upstream version: %s' % latest_upst_ver]
        lines.append('  On %s: %s' % (arch, arch_status))
        lines.append('    %s' % ', '.join(arch_vers))
      else:
        lines.append('  On %s: not used' % arch)

    lines.append('')
    lines.append('Check the latest status for this package, including '
                 'which upstream versions are available, at:\n  %s' %
                 PKG_STATUS_PAGE)
    lines.append('For help upgrading see: %s' % PKG_UPGRADE_PAGE)

    summary = '\n'.join(lines)

    issue = Issue(title=title,
                  summary=summary,
                  status=status,
                  owner=owner,
                  labels=labels,
                  )
    return issue

  def _GetRowTrackerId(self, row):
    """Get the tracker issue id in |row| if it exists, return None otherwise."""
    tracker_val = row[COL_TRACKER]
    if tracker_val:
      return int(tracker_val)

    return None

  def _CreateRowIssue(self, rowIx, row, issue):
    """Create a Tracker issue for |issue|, insert into |row| at |rowIx|"""

    pkg = row[COL_PACKAGE]
    if not self.pretend:
      oper.Info('Creating Tracker issue for package %s with details:\n%s' %
                (pkg, issue))
      issue_id = self.tcomm.CreateTrackerIssue(issue)
      oper.Info('Inserting new Tracker issue %d for package %s' %
                (issue_id, pkg))
      ss_issue_val = self._GenSSLinkToIssue(issue_id)
      self.scomm.ReplaceCellValue(rowIx, self.tracker_col_ix, ss_issue_val)

      oper.Notice('Created Tracker issue %d for row %d, package %r' %
                  (issue_id, rowIx, pkg))
    else:
      oper.Notice('Would create and insert issue for row %d, package %r' %
                  (rowIx, pkg))
      oper.Info('Issue would be as follows:\n%s' % issue)

  def _GenSSLinkToIssue(self, issue_id):
    """Create the spreadsheet hyperlink format for |issue_id|"""
    return '=hyperlink("crosbug.com/%d";"%d")' % (issue_id, issue_id)

  def _ClearRowIssue(self, rowIx, row):
    """Clear the Tracker cell for row at |rowIx|"""

    pkg = row[COL_PACKAGE]
    if not self.pretend:
      oper.Notice('Clearing Tracker issue for package %s' % pkg)
      self.scomm.ClearCellValue(rowIx, self.tracker_col_ix)
    else:
      oper.Notice('Would clear Tracker issue from row %d, package %r' %
                  (rowIx, pkg))


def _CreateOptParser():
  """Create the optparser.parser object for command-line args."""
  usage = 'Usage: %prog [options]'
  epilog = ('\n'
            'Use this script to synchronize tracker issues between the '
            'package status spreadsheet and the chromium-os Tracker.\n'
            'It uses the "Tracker" column of the package spreadsheet. '
            'If a package needs an upgrade and has no tracker issue\n'
            'in that column then a tracker issue is created.  If it '
            'does not need an upgrade then that column is cleared.\n'
            '\n'
            'Credentials must be specified using --cred-file or '
            '--email.  The former has a default value which you can\n'
            'rely on if valid, the latter will prompt for your password.  '
            'If you specify --email you will be given a chance to save\n'
            'your email/password out as a credentials file for next time.\n'
            '\n'
            'Uses spreadsheet key %s (worksheet "%s").\n'
            '\n'
            'Use the --team and --owner options to operate only on '
            'packages assigned to particular owners or teams.\n'
            '\n' %
            (SS_KEY, PKGS_WS_NAME)
            )

  class MyOptParser(optparse.OptionParser):
    """Override default epilog formatter, which strips newlines."""
    def format_epilog(self, formatter):
      return self.epilog

  teamhelp = '[%s]' % ', '.join(Syncer.VALID_TEAMS.keys())

  parser = MyOptParser(usage=usage, epilog=epilog)
  parser.add_option('--cred-file', dest='cred_file', type='string',
                    action='store', default=gdata_lib.CRED_FILE,
                    help='Path to gdata credentials file [default: "%default"]')
  parser.add_option('--email', dest='email', type='string',
                    action='store', default=None,
                    help="Email for Google Doc/Tracker user")
  parser.add_option('--pretend', dest='pretend', action='store_true',
                    default=False,
                    help='Do not make any actual changes.')
  parser.add_option('--team', dest='team', type='string', action='store',
                    default=None,
                    help='Filter by team; colon-separated %s' % teamhelp)
  parser.add_option('--owner', dest='owner', type='string', action='store',
                    default=None,
                    help='Filter by owner; colon-separated chromium.org accts')
  parser.add_option('--verbose', dest='verbose', action='store_true',
                    default=False,
                    help='Enable verbose output (for debugging)')

  return parser

def main():
  """Main function."""
  parser = _CreateOptParser()
  (options, _args) = parser.parse_args()

  oper.verbose = options.verbose

  creds = gdata_lib.Creds(cred_file=options.cred_file, user=options.email)
  tcomm = TrackerComm(creds)
  scomm = SpreadsheetComm(creds, SS_KEY, PKGS_WS_NAME)

  syncer = Syncer(tcomm, scomm,
                  pretend=options.pretend, verbose=options.verbose)

  if options.team:
    syncer.SetTeamFilter(options.team)
  if options.owner:
    syncer.SetOwnerFilter(options.owner)

  try:
    syncer.Sync()
  except SyncError as ex:
    oper.Die(str(ex))

  # If --email, which is only effective when run interactively (because the
  # password must be entered), give the option of saving to a creds file for
  # next time.
  if options.email and options.cred_file:
    prompt = 'Do you want to save credentials to %r?' % options.cred_file
    if 'yes' == cros_lib.YesNoPrompt(default='no', prompt=prompt):
      creds.StoreCreds(options.cred_file)
      oper.Notice('Be sure to save the creds file to the same location'
                  ' outside your chroot so it will also be used with'
                  ' future chroots.')

if __name__ == '__main__':
  main()
