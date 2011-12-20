#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_portage_upgrade.py."""

import os
import sys
import unittest

import gdata.projecthosting.client as gd_client
import gdata.spreadsheet.service as gd_service
import mox

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
import chromite.lib.cros_test_lib as test_lib
import chromite.lib.gdata_lib as gdata_lib
import chromite.lib.upgrade_table as utable
import sync_package_status as sps

# pylint: disable=W0212,R0904

class IssueCommentTest(unittest.TestCase):

  def testInit(self):
    title = 'Greetings, Earthlings'
    text = 'I come in peace.'
    ic = sps.IssueComment(title, text)

    self.assertEquals(title, ic.title)
    self.assertEquals(text, ic.text)


class IssueTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def testInitOverride(self):
    owner = 'somedude@chromium.org'
    status = 'Assigned'
    issue = sps.Issue(owner=owner, status=status)

    self.assertEquals(owner, issue.owner)
    self.assertEquals(status, issue.status)

  def testInitInitFromTracker(self):
    mocked_issue = self.mox.CreateMock(sps.Issue)
    t_issue = 'TheTrackerIssue'

    # Replay script
    mocked_issue.InitFromTracker(t_issue)
    self.mox.ReplayAll()

    # Verify
    sps.Issue.__init__(mocked_issue, tracker_issue=t_issue)
    self.mox.VerifyAll()

  def testInitFromTracker(self):
    # Need to create a dummy Tracker Issue object.
    tissue_id = 123
    tissue_labels = ['Iteration-10', 'Effort-2']
    tissue_owner = 'thedude@chromium.org'
    tissue_status = 'Available'
    tissue_content = 'The summary message'
    tissue_title = 'The Big Title'

    tissue = test_lib.EasyAttr()
    tissue.id = test_lib.EasyAttr(text='http://www/some/path/%d' % tissue_id)
    tissue.label = [test_lib.EasyAttr(text=l) for l in tissue_labels]
    tissue.owner = test_lib.EasyAttr(
      username=test_lib.EasyAttr(text=tissue_owner))
    tissue.status = test_lib.EasyAttr(text=tissue_status)
    tissue.content = test_lib.EasyAttr(text=tissue_content)
    tissue.title = test_lib.EasyAttr(text=tissue_title)

    mocked_issue = self.mox.CreateMock(sps.Issue)

    # Replay script
    mocked_issue.GetTrackerIssueComments(tissue_id).AndReturn([])
    self.mox.ReplayAll()

    # Verify
    sps.Issue.InitFromTracker(mocked_issue, tissue)
    self.mox.VerifyAll()
    self.assertEquals(tissue_id, mocked_issue.id)
    self.assertEquals(tissue_labels, mocked_issue.labels)
    self.assertEquals(tissue_owner, mocked_issue.owner)
    self.assertEquals(tissue_status, mocked_issue.status)
    self.assertEquals(tissue_content, mocked_issue.summary)
    self.assertEquals(tissue_title, mocked_issue.title)
    self.assertEquals([], mocked_issue.comments)


class TrackerCommTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def testInit(self):
    creds = test_lib.EasyAttr(user='dude', password='shhh')
    mocked_itclient = self.mox.CreateMock(gd_client.ProjectHostingClient)
    mocked_tcomm = self.mox.CreateMock(sps.TrackerComm)

    self.mox.StubOutWithMock(gd_client.ProjectHostingClient, '__new__')

    # Replay script
    gd_client.ProjectHostingClient.__new__(
      gd_client.ProjectHostingClient).AndReturn(mocked_itclient)
    mocked_itclient.ClientLogin(creds.user, creds.password,
                                source='package_status_upgrade',
                                service='code',
                                account_type='GOOGLE')
    self.mox.ReplayAll()

    # Verify
    sps.TrackerComm.__init__(mocked_tcomm, creds)
    self.mox.VerifyAll()
    self.assertEquals(mocked_tcomm.creds, creds)

  def testGetTrackerIssueById(self):
    mocked_itclient = self.mox.CreateMock(gd_client.ProjectHostingClient)
    mocked_tcomm = self.mox.CreateMock(sps.TrackerComm)
    mocked_tcomm.it_client = mocked_itclient

    self.mox.StubOutWithMock(gd_client.Query, '__new__')
    self.mox.StubOutWithMock(sps.Issue, '__new__')

    issue_id = 12345
    feed = test_lib.EasyAttr(entry=['hi', 'there'])

    # Replay script
    gd_client.Query.__new__(gd_client.Query,
                            issue_id=str(issue_id)).AndReturn('Q')
    mocked_itclient.get_issues(sps.PROJECT_NAME, query='Q').AndReturn(feed)
    sps.Issue.__new__(sps.Issue, 'hi').AndReturn('EndResult')
    self.mox.ReplayAll()

    # Verify
    result = sps.TrackerComm.GetTrackerIssueById(mocked_tcomm, issue_id)
    self.mox.VerifyAll()
    self.assertEquals('EndResult', result)

  def testCreateTrackerIssue(self):
    creds = test_lib.EasyAttr(user='dude', password='shhh')
    mocked_itclient = self.mox.CreateMock(gd_client.ProjectHostingClient)
    mocked_tcomm = self.mox.CreateMock(sps.TrackerComm)
    mocked_tcomm.it_client = mocked_itclient
    mocked_tcomm.creds = creds

    issue = test_lib.EasyAttr(title='TheTitle',
                     summary='TheSummary',
                     status='TheStatus',
                     owner='TheOwner',
                     labels='TheLabels')

    # Replay script
    issue_id = test_lib.EasyAttr(id=test_lib.EasyAttr(text='foo/bar/123'))
    mocked_itclient.add_issue(project_name=sps.PROJECT_NAME,
                              title=issue.title,
                              content=issue.summary,
                              author=creds.user,
                              status=issue.status,
                              owner=issue.owner,
                              labels=issue.labels,
                              ).AndReturn(issue_id)
    self.mox.ReplayAll()

    # Verify
    result = sps.TrackerComm.CreateTrackerIssue(mocked_tcomm, issue)
    self.mox.VerifyAll()
    self.assertEquals(123, result)

  def testAppendTrackerIssueById(self):
    creds = test_lib.EasyAttr(user='dude', password='shhh')
    mocked_itclient = self.mox.CreateMock(gd_client.ProjectHostingClient)
    mocked_tcomm = self.mox.CreateMock(sps.TrackerComm)
    mocked_tcomm.it_client = mocked_itclient
    mocked_tcomm.creds = creds

    issue_id = 54321
    comment = 'TheComment'

    # Replay script
    mocked_itclient.update_issue(project_name=sps.PROJECT_NAME,
                                 issue_id=issue_id,
                                 author=creds.user,
                                 comment=comment)
    self.mox.ReplayAll()

    # Verify
    result = sps.TrackerComm.AppendTrackerIssueById(mocked_tcomm, issue_id,
                                                    comment)
    self.mox.VerifyAll()
    self.assertEquals(issue_id, result)


class SpreadsheetCommTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def testInit(self):
    creds = test_lib.EasyAttr(user='dude', password='shhh')
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_scomm.creds = creds

    ss_key = 'TheSSKey'
    ws_name = 'TheWSName'
    ws_key = 'TheWSKey'
    cols = 'TheColumns'

    # Replay script
    mocked_scomm._LoginWithUserPassword(creds.user, creds.password)
    mocked_scomm._GetWorksheetKey(ss_key, ws_name).AndReturn(ws_key)
    mocked_scomm._GetColumns().AndReturn(cols)
    self.mox.ReplayAll()

    # Verify
    sps.SpreadsheetComm.__init__(mocked_scomm, creds, ss_key, ws_name)
    self.mox.VerifyAll()
    self.assertEquals(ss_key, mocked_scomm.ss_key)
    self.assertEquals(ws_key, mocked_scomm.ws_key)
    self.assertEquals(cols, mocked_scomm.columns)

  def testLoginWithUserPassword(self):
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_gdclient = self.mox.CreateMock(gdata_lib.RetrySpreadsheetsService)

    self.mox.StubOutWithMock(gdata_lib.RetrySpreadsheetsService, '__new__')

    user = 'dude'
    password = 'shhh'

    # Replay script
    gdata_lib.RetrySpreadsheetsService.__new__(
        gdata_lib.RetrySpreadsheetsService).AndReturn(mocked_gdclient)
    mocked_gdclient.ProgrammaticLogin()
    self.mox.ReplayAll()

    # Verify
    sps.SpreadsheetComm._LoginWithUserPassword(mocked_scomm, user, password)
    self.mox.VerifyAll()
    self.assertEquals(user, mocked_gdclient.email)
    self.assertEquals(password, mocked_gdclient.password)
    self.assertEquals(mocked_gdclient, mocked_scomm.gd_client)

  def testGetWorksheetKey(self):
    mocked_gdclient = self.mox.CreateMock(gd_service.SpreadsheetsService)
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_scomm.gd_client = mocked_gdclient

    ss_key = 'TheSSKey'
    ws_name = 'TheWSName'
    ws_key = 'TheWSKey'

    entrylist = [
      test_lib.EasyAttr(title=test_lib.EasyAttr(text='Foo'), id='NotImportant'),
      test_lib.EasyAttr(title=test_lib.EasyAttr(text=ws_name),
               id=test_lib.EasyAttr(text='/some/path/%s' % ws_key)),
      test_lib.EasyAttr(title=test_lib.EasyAttr(text='Bar'), id='NotImportant'),
      ]
    feed = test_lib.EasyAttr(entry=entrylist)

    # Replay script
    mocked_gdclient.GetWorksheetsFeed(ss_key).AndReturn(feed)
    self.mox.ReplayAll()

    # Verify
    sps.SpreadsheetComm._GetWorksheetKey(mocked_scomm, ss_key, ws_name)
    self.mox.VerifyAll()

  def testGetColumns(self):
    mocked_gdclient = self.mox.CreateMock(gd_service.SpreadsheetsService)
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_scomm.gd_client = mocked_gdclient
    mocked_scomm.ss_key = 'TheSSKey'
    mocked_scomm.ws_key = 'TheWSKey'

    self.mox.StubOutWithMock(gd_service.CellQuery, '__new__')

    columns = ['These', 'Are', 'Column', 'Names']
    entrylist = [test_lib.EasyAttr(
      content=test_lib.EasyAttr(text=c)) for c in columns]
    feed = test_lib.EasyAttr(entry=entrylist)
    query = {'max-row': '1'}

    # Replay script
    gd_service.CellQuery.__new__(gd_service.CellQuery).AndReturn({})
    mocked_gdclient.GetCellsFeed(mocked_scomm.ss_key, mocked_scomm.ws_key,
                                 query=query).AndReturn(feed)
    self.mox.ReplayAll()

    # Verify
    result = sps.SpreadsheetComm._GetColumns(mocked_scomm)
    self.mox.VerifyAll()
    self.assertEquals(columns, result)

  def testGetColumnIndex(self):
    # Note that spreadsheet column indices start at 1.
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_scomm.columns = ['These', 'Are', 'Column', 'Names']

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = sps.SpreadsheetComm.GetColumnIndex(mocked_scomm, 'Are')
    self.mox.VerifyAll()
    self.assertEquals(2, result)

  def testGetAllRowsAsDicts(self):
    mocked_gdclient = self.mox.CreateMock(gd_service.SpreadsheetsService)
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_scomm.gd_client = mocked_gdclient
    mocked_scomm.ss_key = 'TheSSKey'
    mocked_scomm.ws_key = 'TheWSKey'

    rows = [
      { 'Greeting': 'Hi', 'Name': 'George', },
      { 'Greeting': 'Howdy', 'Name': 'Billy Bob', },
      { 'Greeting': 'Yo', 'Name': 'Dane', },
      ]
    # Construct a simulation of the funky gdata representation of rows.
    entrylist = []
    for row in rows:
      entryhash = dict([(k, test_lib.EasyAttr(text=v))
                        for (k, v) in row.items()])
      entrylist.append(test_lib.EasyAttr(custom=entryhash))
    feed = test_lib.EasyAttr(entry=entrylist)

    # Replay script
    mocked_gdclient.GetListFeed(mocked_scomm.ss_key,
                                mocked_scomm.ws_key).AndReturn(feed)
    self.mox.ReplayAll()

    # Verify
    result = sps.SpreadsheetComm.GetAllRowsAsDicts(mocked_scomm)
    self.mox.VerifyAll()
    self.assertEquals(rows, result)

  def testReplaceCellValue(self):
    mocked_gdclient = self.mox.CreateMock(gd_service.SpreadsheetsService)
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_scomm.gd_client = mocked_gdclient
    mocked_scomm.ss_key = 'TheSSKey'
    mocked_scomm.ws_key = 'TheWSKey'

    rowIx = 14
    colIx = 4
    val = 'TheValue'

    # Replay script
    mocked_gdclient.UpdateCell(rowIx, colIx, val,
                               mocked_scomm.ss_key, mocked_scomm.ws_key)
    self.mox.ReplayAll()

    # Verify
    sps.SpreadsheetComm.ReplaceCellValue(mocked_scomm, rowIx, colIx, val)
    self.mox.VerifyAll()

  def testClearCellValue(self):
    mocked_gdclient = self.mox.CreateMock(gd_service.SpreadsheetsService)
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_scomm.gd_client = mocked_gdclient
    mocked_scomm.ss_key = 'TheSSKey'
    mocked_scomm.ws_key = 'TheWSKey'

    rowIx = 14
    colIx = 4

    # Replay script
    mocked_scomm.ReplaceCellValue(rowIx, colIx, None)
    self.mox.ReplayAll()

    # Verify
    sps.SpreadsheetComm.ClearCellValue(mocked_scomm, rowIx, colIx)
    self.mox.VerifyAll()


class SyncerTest(test_lib.MoxTestCase):

  col_amd64 = utable.UpgradeTable.GetColumnName(utable.UpgradeTable.COL_STATE,
                                                'amd64')
  col_amd64 = gdata_lib.PrepColNameForSS(col_amd64)
  col_arm = utable.UpgradeTable.GetColumnName(utable.UpgradeTable.COL_STATE,
                                              'arm')
  col_arm = gdata_lib.PrepColNameForSS(col_arm)
  col_x86 = utable.UpgradeTable.GetColumnName(utable.UpgradeTable.COL_STATE,
                                              'x86')
  col_x86 = gdata_lib.PrepColNameForSS(col_x86)

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def testInit(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_tcomm = self.mox.CreateMock(sps.TrackerComm)

    tracker_col_ix = 8

    # Replay script
    mocked_scomm.GetColumnIndex('Tracker').AndReturn(tracker_col_ix)
    self.mox.ReplayAll()

    # Verify
    sps.Syncer.__init__(mocked_syncer, mocked_tcomm, mocked_scomm)
    self.mox.VerifyAll()
    self.assertEquals(mocked_scomm, mocked_syncer.scomm)
    self.assertEquals(mocked_tcomm, mocked_syncer.tcomm)
    self.assertEquals(tracker_col_ix, mocked_syncer.tracker_col_ix)
    self.assertEquals(None, mocked_syncer.teams)
    self.assertEquals(None, mocked_syncer.owners)
    self.assertEquals(False, mocked_syncer.pretend)
    self.assertEquals(False, mocked_syncer.verbose)

  def testReduceTeamName(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    tests = {
      'build/bdavirro': 'build',
      'build/rtc': 'build',
      'build': 'build',
      'UI/zel': 'ui',
      'UI': 'ui',
      'Build': 'build',
      None: None,
      }

    # Replay script
    self.mox.ReplayAll()

    # Verify
    for key in tests:
      result = sps.Syncer._ReduceTeamName(mocked_syncer, key)
      self.assertEquals(tests[key], result)
    self.mox.VerifyAll()

  def testReduceOwnerName(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    tests = {
      'joe': 'joe',
      'Joe': 'joe',
      'joe@chromium.org': 'joe',
      'Joe@chromium.org': 'joe',
      'Joe.Bob@chromium.org': 'joe.bob',
      None: None,
      }

    # Replay script
    self.mox.ReplayAll()

    # Verify
    for key in tests:
      result = sps.Syncer._ReduceOwnerName(mocked_syncer, key)
      self.assertEquals(tests[key], result)
    self.mox.VerifyAll()

  def testSetTeamFilterOK(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    tests = {
      'build:system:ui': set(['build', 'system', 'ui']),
      'Build:system:UI': set(['build', 'system', 'ui']),
      'kernel': set(['kernel']),
      'KERNEL': set(['kernel']),
      None: None,
      '': None,
      }

    # Replay script
    for test in tests:
      if test:
        for team in test.split(':'):
          reduced = sps.Syncer._ReduceTeamName(mocked_syncer, team)
          mocked_syncer._ReduceTeamName(team).AndReturn(reduced)
    self.mox.ReplayAll()

    # Verify
    for test in tests:
      sps.Syncer.SetTeamFilter(mocked_syncer, test)
      self.assertEquals(tests[test], mocked_syncer.teams)
    self.mox.VerifyAll()

  def testSetTeamFilterError(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    # "systems" is not valid (should be "system")
    teamarg = 'build:systems'

    # Replay script
    mocked_syncer._ReduceTeamName('build').AndReturn('build')
    mocked_syncer._ReduceTeamName('systems').AndReturn('systems')
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      self.assertRaises(SystemExit, sps.Syncer.SetTeamFilter,
                        mocked_syncer, teamarg)
    self.mox.VerifyAll()

  def testSetOwnerFilter(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    tests = {
      'joe:bill:bob': set(['joe', 'bill', 'bob']),
      'Joe:Bill:BOB': set(['joe', 'bill', 'bob']),
      'joe@chromium.org:bill:bob': set(['joe', 'bill', 'bob']),
      'joe': set(['joe']),
      'joe@chromium.org': set(['joe']),
      '': None,
      None: None,
      }

    # Replay script
    for test in tests:
      if test:
        for owner in test.split(':'):
          reduced = sps.Syncer._ReduceOwnerName(mocked_syncer, owner)
          mocked_syncer._ReduceOwnerName(owner).AndReturn(reduced)
    self.mox.ReplayAll()

    # Verify
    for test in tests:
      sps.Syncer.SetOwnerFilter(mocked_syncer, test)
      self.assertEquals(tests[test], mocked_syncer.owners)
    self.mox.VerifyAll()

  def testRowPassesFilters(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    row1 = { sps.COL_TEAM: 'build', sps.COL_OWNER: 'joe' }
    row2 = { sps.COL_TEAM: 'build', sps.COL_OWNER: 'bob' }
    row3 = { sps.COL_TEAM: 'build', sps.COL_OWNER: None }
    row4 = { sps.COL_TEAM: None, sps.COL_OWNER: None }

    teams1 = set(['build'])
    teams2 = set(['kernel'])
    teams3 = set(['build', 'ui'])

    owners1 = set(['joe'])
    owners2 = set(['bob'])
    owners3 = set(['joe', 'dan'])

    tests = [
      { 'row': row1, 'teams': None, 'owners': None, 'result': True },
      { 'row': row1, 'teams': teams1, 'owners': None, 'result': True },
      { 'row': row1, 'teams': teams2, 'owners': None, 'result': False },
      { 'row': row1, 'teams': teams3, 'owners': None, 'result': True },
      { 'row': row1, 'teams': teams1, 'owners': owners1, 'result': True },
      { 'row': row1, 'teams': None, 'owners': owners2, 'result': False },
      { 'row': row1, 'teams': None, 'owners': owners3, 'result': True },

      { 'row': row2, 'teams': None, 'owners': None, 'result': True },
      { 'row': row2, 'teams': teams1, 'owners': None, 'result': True },
      { 'row': row2, 'teams': teams2, 'owners': None, 'result': False },
      { 'row': row2, 'teams': teams3, 'owners': None, 'result': True },
      { 'row': row2, 'teams': teams1, 'owners': owners1, 'result': False },
      { 'row': row2, 'teams': None, 'owners': owners2, 'result': True },
      { 'row': row2, 'teams': None, 'owners': owners3, 'result': False },

      { 'row': row3, 'teams': None, 'owners': None, 'result': True },
      { 'row': row3, 'teams': teams1, 'owners': None, 'result': True },
      { 'row': row3, 'teams': teams2, 'owners': None, 'result': False },
      { 'row': row3, 'teams': teams3, 'owners': None, 'result': True },
      { 'row': row3, 'teams': teams1, 'owners': owners1, 'result': False },
      { 'row': row3, 'teams': None, 'owners': owners2, 'result': False },
      { 'row': row3, 'teams': None, 'owners': owners3, 'result': False },

      { 'row': row4, 'teams': None, 'owners': None, 'result': True },
      { 'row': row4, 'teams': teams1, 'owners': None, 'result': False },
      { 'row': row4, 'teams': teams1, 'owners': owners1, 'result': False },
      { 'row': row4, 'teams': None, 'owners': owners2, 'result': False },
      ]

    # Replay script
    for test in tests:
      done = False

      if test['teams']:
        row_team = test['row'][sps.COL_TEAM]
        mocked_syncer._ReduceTeamName(row_team).AndReturn(row_team)
        done = row_team not in test['teams']

      if not done and test['owners']:
        row_owner = test['row'][sps.COL_OWNER]
        mocked_syncer._ReduceOwnerName(row_owner).AndReturn(row_owner)
    self.mox.ReplayAll()

    # Verify
    for test in tests:
      mocked_syncer.teams = test['teams']
      mocked_syncer.owners = test['owners']
      result = sps.Syncer._RowPassesFilters(mocked_syncer, test['row'])

      msg = ('Expected following row to %s filter, but it did not:\n%r' %
             ('pass' if test['result'] else 'fail', test['row']))
      msg += '\n  Using teams filter : %r' % mocked_syncer.teams
      msg += '\n  Using owners filter: %r' % mocked_syncer.owners
      self.assertEquals(test['result'], result, msg)
    self.mox.VerifyAll()

  def testSyncNewIssues(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_tcomm = self.mox.CreateMock(sps.TrackerComm)
    mocked_syncer.scomm = mocked_scomm
    mocked_syncer.tcomm = mocked_tcomm

    rows = [
      { sps.COL_PACKAGE: 'd/f', sps.COL_TEAM: 'build', sps.COL_OWNER: None },
      { sps.COL_PACKAGE: 'd/f', sps.COL_TEAM: 'build', sps.COL_OWNER: 'joe' },
      ]

    # Replay script
    mocked_scomm.GetAllRowsAsDicts().AndReturn(rows)

    for ix in xrange(len(rows)):
      mocked_syncer._RowPassesFilters(rows[ix]).AndReturn(True)
      mocked_syncer._GenIssueForRow(rows[ix]).AndReturn('NewIssue%d' % ix)
      mocked_syncer._GetRowTrackerId(rows[ix]).AndReturn(None)
      mocked_syncer._CreateRowIssue(ix + 2, rows[ix], 'NewIssue%d' % ix)
    self.mox.ReplayAll()

    # Verify
    sps.Syncer.Sync(mocked_syncer)
    self.mox.VerifyAll()

  def testSyncClearIssues(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_tcomm = self.mox.CreateMock(sps.TrackerComm)
    mocked_syncer.scomm = mocked_scomm
    mocked_syncer.tcomm = mocked_tcomm

    rows = [
      { sps.COL_PACKAGE: 'd/f', sps.COL_TEAM: 'build', sps.COL_OWNER: None },
      { sps.COL_PACKAGE: 'd/f', sps.COL_TEAM: 'build', sps.COL_OWNER: 'joe' },
      ]

    # Replay script
    mocked_scomm.GetAllRowsAsDicts().AndReturn(rows)

    for ix in xrange(len(rows)):
      mocked_syncer._RowPassesFilters(rows[ix]).AndReturn(True)
      mocked_syncer._GenIssueForRow(rows[ix]).AndReturn(None)
      mocked_syncer._GetRowTrackerId(rows[ix]).AndReturn(123 + ix)
      mocked_syncer._ClearRowIssue(ix + 2, rows[ix])
    self.mox.ReplayAll()

    # Verify
    sps.Syncer.Sync(mocked_syncer)
    self.mox.VerifyAll()

  def testSyncFilteredOut(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_tcomm = self.mox.CreateMock(sps.TrackerComm)
    mocked_syncer.scomm = mocked_scomm
    mocked_syncer.tcomm = mocked_tcomm

    rows = [
      { sps.COL_PACKAGE: 'd/f', sps.COL_TEAM: 'build', sps.COL_OWNER: None },
      { sps.COL_PACKAGE: 'd/f', sps.COL_TEAM: 'build', sps.COL_OWNER: 'joe' },
      ]

    # Replay script
    mocked_scomm.GetAllRowsAsDicts().AndReturn(rows)

    for ix in xrange(len(rows)):
      mocked_syncer._RowPassesFilters(rows[ix]).AndReturn(False)
    self.mox.ReplayAll()

    # Verify
    sps.Syncer.Sync(mocked_syncer)
    self.mox.VerifyAll()

  def testGetRowValue(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    row = {
      self.col_amd64: 'ABC',
      self.col_arm: 'XYZ',
      self.col_x86: 'FooBar',
      sps.COL_TEAM: 'build',
      }

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = sps.Syncer._GetRowValue(mocked_syncer, row,
                                     'stateonamd64', 'amd64')
    self.assertEquals('ABC', result)
    result = sps.Syncer._GetRowValue(mocked_syncer, row,
                                     'stateonarm', 'arm')
    self.assertEquals('XYZ', result)
    result = sps.Syncer._GetRowValue(mocked_syncer, row,
                                     'stateonamd64', 'amd64')
    self.assertEquals('ABC', result)
    result = sps.Syncer._GetRowValue(mocked_syncer, row, sps.COL_TEAM)
    self.assertEquals('build', result)
    self.mox.VerifyAll()

  def _TestGenIssueForRowNeedsUpgrade(self, row):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    # Replay script
    for arch in sps.ARCHES:
      state = sps.Syncer._GetRowValue(mocked_syncer, row,
                                      utable.UpgradeTable.COL_STATE, arch)
      mocked_syncer._GetRowValue(row, utable.UpgradeTable.COL_STATE,
                                 arch).AndReturn(state)
    red_team = sps.Syncer._ReduceTeamName(mocked_syncer, row[sps.COL_TEAM])
    mocked_syncer._ReduceTeamName(row[sps.COL_TEAM]).AndReturn(red_team)
    red_owner = sps.Syncer._ReduceOwnerName(mocked_syncer, row[sps.COL_OWNER])
    mocked_syncer._ReduceOwnerName(row[sps.COL_OWNER]).AndReturn(red_owner)
    for arch in sps.ARCHES:
      mocked_syncer._GetRowValue(row, utable.UpgradeTable.COL_CURRENT_VER,
                                 arch).AndReturn('1')
      mocked_syncer._GetRowValue(row,
                                 utable.UpgradeTable.COL_STABLE_UPSTREAM_VER,
                                 arch).AndReturn('2')
      mocked_syncer._GetRowValue(row,
                                 utable.UpgradeTable.COL_LATEST_UPSTREAM_VER,
                                 arch).AndReturn('3')
    self.mox.ReplayAll()

    # Verify
    result = sps.Syncer._GenIssueForRow(mocked_syncer, row)
    self.mox.VerifyAll()
    return result

  def testGenIssueForRowNeedsUpgrade1(self):
    row = {
      self.col_amd64: utable.UpgradeTable.STATE_NEEDS_UPGRADE,
      self.col_arm: 'Not important',
      self.col_x86: 'Not important',
      sps.COL_TEAM: 'build',
      sps.COL_OWNER: None,
      sps.COL_PACKAGE: 'dev/foo',
      }

    result = self._TestGenIssueForRowNeedsUpgrade(row)
    self.assertEquals(None, result.owner)
    self.assertEquals(0, result.id)
    self.assertEquals('Untriaged', result.status)

  def testGenIssueForRowNeedsUpgrade2(self):
    row = {
      self.col_amd64: utable.UpgradeTable.STATE_NEEDS_UPGRADE,
      self.col_arm: utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_PATCHED,
      self.col_x86: 'Not important',
      sps.COL_TEAM: 'build',
      sps.COL_OWNER: 'joe',
      sps.COL_PACKAGE: 'dev/foo',
      }

    result = self._TestGenIssueForRowNeedsUpgrade(row)
    self.assertEquals('joe@chromium.org', result.owner)
    self.assertEquals(0, result.id)
    self.assertEquals('Available', result.status)

  def testGenIssueForRowNeedsUpgrade3(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    row = {
      self.col_amd64: utable.UpgradeTable.STATE_NEEDS_UPGRADE,
      self.col_arm: utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_PATCHED,
      self.col_x86: 'Not important',
      sps.COL_TEAM: None,
      sps.COL_OWNER: 'joe',
      sps.COL_PACKAGE: 'dev/foo',
      }

    # Replay script
    for arch in sps.ARCHES:
      state = sps.Syncer._GetRowValue(mocked_syncer, row,
                                      utable.UpgradeTable.COL_STATE, arch)
      mocked_syncer._GetRowValue(row, utable.UpgradeTable.COL_STATE,
                                 arch).AndReturn(state)
    reduced = sps.Syncer._ReduceTeamName(mocked_syncer, row[sps.COL_TEAM])
    mocked_syncer._ReduceTeamName(row[sps.COL_TEAM]).AndReturn(reduced)
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      self.assertRaises(RuntimeError, sps.Syncer._GenIssueForRow,
                        mocked_syncer, row)
    self.mox.VerifyAll()

  def testGenIssueForRowNoUpgrade(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    row = {
      self.col_amd64: 'Not important',
      self.col_arm: 'Not important',
      self.col_x86: 'Not important',
      sps.COL_TEAM: None,
      sps.COL_OWNER: 'joe',
      sps.COL_PACKAGE: 'dev/foo',
      }

    # Replay script
    for arch in sps.ARCHES:
      state = sps.Syncer._GetRowValue(mocked_syncer, row,
                                      utable.UpgradeTable.COL_STATE, arch)
      mocked_syncer._GetRowValue(row, utable.UpgradeTable.COL_STATE,
                                 arch).AndReturn(state)
    self.mox.ReplayAll()

    # Verify
    result = sps.Syncer._GenIssueForRow(mocked_syncer, row)
    self.mox.VerifyAll()
    self.assertEquals(None, result)

  def testGetRowTrackerId(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    row = { sps.COL_TRACKER: '321' }

    # Replay script
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      result = sps.Syncer._GetRowTrackerId(mocked_syncer, row)
    self.mox.VerifyAll()
    self.assertEquals(321, result)

  def testCreateRowIssuePretend(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_syncer.pretend = True

    row = { sps.COL_PACKAGE: 'dev/foo' }

    # Replay script
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      sps.Syncer._CreateRowIssue(mocked_syncer, 5, row, 'some_issue')
    self.mox.VerifyAll()

  def testCreateRowIssue(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_tcomm = self.mox.CreateMock(sps.TrackerComm)
    mocked_syncer.scomm = mocked_scomm
    mocked_syncer.tcomm = mocked_tcomm
    mocked_syncer.tracker_col_ix = 8
    mocked_syncer.pretend = False

    row_ix = 5
    row = { sps.COL_PACKAGE: 'dev/foo' }
    issue = 'SomeIssue'
    issue_id = 234
    ss_issue_val = 'Hyperlink%d' % issue_id

    # Replay script
    mocked_tcomm.CreateTrackerIssue(issue).AndReturn(issue_id)
    mocked_syncer._GenSSLinkToIssue(issue_id).AndReturn(ss_issue_val)
    mocked_scomm.ReplaceCellValue(row_ix, mocked_syncer.tracker_col_ix,
                                  ss_issue_val)
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      sps.Syncer._CreateRowIssue(mocked_syncer, row_ix, row, issue)
    self.mox.VerifyAll()

  def testGenSSLinkToIssue(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    issue_id = 123

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = sps.Syncer._GenSSLinkToIssue(mocked_syncer, issue_id)
    self.mox.VerifyAll()
    self.assertEquals('=hyperlink("crosbug.com/123";"123")', result)

  def testClearRowIssue(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_syncer.scomm = mocked_scomm
    mocked_syncer.tracker_col_ix = 8
    mocked_syncer.pretend = False

    row_ix = 44
    row = { sps.COL_PACKAGE: 'dev/foo' }

    # Replay script
    mocked_scomm.ClearCellValue(row_ix, mocked_syncer.tracker_col_ix)
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      sps.Syncer._ClearRowIssue(mocked_syncer, row_ix, row)
    self.mox.VerifyAll()

  def testClearRowIssuePretend(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_scomm = self.mox.CreateMock(sps.SpreadsheetComm)
    mocked_syncer.scomm = mocked_scomm
    mocked_syncer.tracker_col_ix = 8
    mocked_syncer.pretend = True

    row_ix = 44
    row = { sps.COL_PACKAGE: 'dev/foo' }

    # Replay script
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      sps.Syncer._ClearRowIssue(mocked_syncer, row_ix, row)
    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()
