#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for GerritHelper.  Needs to have mox installed."""

import mox
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gerrit


# pylint: disable=W0212,R0904
class GerritHelperTest(cros_test_lib.MoxTestCase):

  def setUp(self):
    self.footer_template = (
      '{"type":"stats","rowCount":%(count)i,"runTimeMilliseconds":205}')
    self.results = (
        '{"project":"chromiumos/platform/init","branch":"master",'
        '"id":"Iee5c89d929f1850d7d4e1a4ff5f21adda800025e",'
        '"number":"1111",'
        '"subject":"init commit",'
        '"owner":{"name":"Init master","email":"init@chromium.org"},'
        '"currentPatchSet":{"number":"2","ref":"refs/changes/72/5172/1",'
            '"revision":"ff10979dd360e75ff21f5cf53b7f8647578785ef"},'
        '"url":"http://gerrit.chromium.org/gerrit/1111",'
        '"lastUpdated":1311024429,'
        '"sortKey":"00166e8700001051",'
        '"open":true,"'
        'status":"NEW"}'
        '\n'
        '{"project":"chromiumos/manifests","branch":"master",'
        '"id":"Iee5c89d929f1850d7d4e1a4ff5f21adda800025d",'
        '"number":"1111",'
        '"subject":"Test for filtered repos",'
        '"owner":{"name":"Init master","email":"init@chromium.org"},'
        '"currentPatchSet":{"number":"2","ref":"refs/changes/72/5172/1",'
            '"revision":"ff10979dd360e75ff21f5cf53b7f8647578785ef"},'
        '"url":"http://gerrit.chromium.org/gerrit/1110",'
        '"lastUpdated":1311024429,'
        '"sortKey":"00166e8700001051",'
        '"open":true,"'
        'status":"NEW"}'
        '\n'
        '{"project":"tacos/chromite","branch":"master",'
        '"id":"Iee5c89d929f1850d7d4e1a4ff5f21adda800025f",'
        '"currentPatchSet":{"number":"2","ref":"refs/changes/72/5172/1",'
            '"revision":"ff10979dd360e75ff21f5cf53b7f8647578785ef"},'
        '"number":"1112",'
        '"subject":"chromite commit",'
        '"owner":{"name":"Chromite Master","email":"chromite@chromium.org"},'
        '"url":"http://gerrit.chromium.org/gerrit/1112",'
        '"lastUpdated":1311024529,'
        '"sortKey":"00166e8700001052",'
        '"open":true,"'
        'status":"NEW"}\n'
        ) + self.footer_template % {'count':1}
    self.merged_record = (
        '{"project":"tacos/chromite","branch":"master",'
        '"id":"Iee5c89d929f1850d7d4e1a4ff5f21adda8000250",'
        '"currentPatchSet":{"number":"2","ref":"refs/changes/72/5172/1",'
            '"revision":"ff10979dd360e75ff21f5cf53b7f8647578785ea"},'
        '"number":"1112",'
        '"subject":"chromite commit",'
        '"owner":{"name":"Chromite Master","email":"chromite@chromium.org"},'
        '"url":"http://gerrit.chromium.org/gerrit/1112",'
        '"lastUpdated":1311024529,'
        '"sortKey":"00166e8700001052",'
        '"open":true,"'
        'status":"MERGED"}\n'
    )
    self.merged_change = self.merged_record + self.footer_template % {'count':1}
    self.no_results = self.footer_template % {'count':0}

  def _GetHelper(self, remote=constants.EXTERNAL_REMOTE):
    return gerrit.GerritHelper.FromRemote(remote)

  def testGerritQueryTruncation(self):
    """Verify that we detect gerrit truncating our query, and handle it."""
    query1 = self.mox.CreateMock(cros_build_lib.CommandResult)
    query1.output = "%s%s" % (self.merged_record * 500,
                              self.footer_template % {'count':500})
    query2 = self.mox.CreateMock(cros_build_lib.CommandResult)
    query2.output = '%s%s' % (self.merged_record * 313,
                              self.footer_template % {'count':313})
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    cros_build_lib.RunCommand(mox.In('gerrit.chromium.org'),
                              redirect_stdout=True).AndReturn(query1)
    cros_build_lib.RunCommand(mox.In('resume_sortkey:00166e8700001052'),
                              redirect_stdout=True).AndReturn(query2)
    self.mox.ReplayAll()
    helper = self._GetHelper()
    changes = helper.Query('monkeys')
    self.assertEqual(len(changes), 813)

  def testParseFakeResults(self):
    """Parses our own fake gerrit query results to verify we parse correctly."""
    fake_result = self.mox.CreateMock(cros_build_lib.CommandResult)
    fake_result.output = self.results
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    cros_build_lib.RunCommand(mox.In('gerrit.chromium.org'),
                              redirect_stdout=True).AndReturn(fake_result)
    self.mox.ReplayAll()
    helper = self._GetHelper()
    changes = helper.Query("monkey")
    self.assertEqual(set(x.change_id for x in changes),
                     set(['Iee5c89d929f1850d7d4e1a4ff5f21adda800025f',
                          'Iee5c89d929f1850d7d4e1a4ff5f21adda800025d',
                          'Iee5c89d929f1850d7d4e1a4ff5f21adda800025e']))
    self.mox.VerifyAll()

  def testParseFakeResultsWithInternalURL(self):
    """Parses our own fake gerrit query results but sets internal bit."""
    fake_result = self.mox.CreateMock(cros_build_lib.CommandResult)
    fake_result.output = self.results
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    cros_build_lib.RunCommand(mox.In('gerrit-int.chromium.org'),
                              redirect_stdout=True).AndReturn(fake_result)
    self.mox.ReplayAll()
    helper = self._GetHelper(constants.INTERNAL_REMOTE)
    changes = helper.Query("monkeys")
    self.assertEqual(set(x.change_id for x in changes),
                     set(['Iee5c89d929f1850d7d4e1a4ff5f21adda800025f',
                          'Iee5c89d929f1850d7d4e1a4ff5f21adda800025d',
                          'Iee5c89d929f1850d7d4e1a4ff5f21adda800025e']))
    self.mox.VerifyAll()

  def _PrintChanges(self, changes):
    """Deep print of an array of changes."""
    for change in changes:
      print change

  def testRealCommandWorks(self):
    """This is just a sanity test that the command is valid.

    Runs the command and prints out the changes.  Should not throw an exception.
    """
    helper = self._GetHelper()
    change = helper.QuerySingleRecord('2')
    self.assertEqual(change.gerrit_number, '2')
    self.assertEqual(change.change_id,
                     'Ie787aca962560d8df2fcbfc3ec0a0c5b963235b1')
    self.assertEqual(change.project, 'playground/bar')

  def testInternalCommandWorks(self):
    """This is just a sanity test that the internal command is valid.

    Runs the command and prints out the changes.  Should not throw an exception.
    """
    helper = self._GetHelper(constants.INTERNAL_REMOTE)
    change = helper.QuerySingleRecord('1')
    self.assertEqual(change.gerrit_number, '1')
    self.assertEqual(change.change_id,
                     'Idd5056fbd4d6d10525104edc0aa67ee02aaf3a91')
    self.assertEqual(change.project, 'playground/foo')

  def testIsChangeCommitted(self):
    """Tests that we can parse a json to check if a change is committed."""
    changeid = 'Ia6e663415c004bdaa77101a7e3258657598b0468'
    changeid_bad = 'I97663415c004bdaa77101a7e3258657598b0468'
    fake_result_from_gerrit = self.mox.CreateMock(cros_build_lib.CommandResult)
    fake_result_from_gerrit.output = self.merged_change
    fake_bad_result_from_gerrit = self.mox.CreateMock(
        cros_build_lib.CommandResult)
    fake_bad_result_from_gerrit.output = self.no_results
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    cros_build_lib.RunCommand(mox.In('change:%s' % changeid),
                              redirect_stdout=True).AndReturn(
                                  fake_result_from_gerrit)
    cros_build_lib.RunCommand(mox.In('change:%s' % changeid_bad),
                              redirect_stdout=True).AndReturn(
                                  fake_bad_result_from_gerrit)
    self.mox.ReplayAll()
    helper = self._GetHelper()
    self.assertTrue(helper.IsChangeCommitted(changeid))
    self.assertFalse(helper.IsChangeCommitted(changeid_bad, must_match=False))
    self.mox.VerifyAll()

  def testCanRunIsChangeCommand(self):
    """Sanity test for IsChangeCommitted to make sure it works."""
    changeid = 'Ia6e663415c004bdaa77101a7e3258657598b0468'
    helper = self._GetHelper()
    self.assertTrue(helper.IsChangeCommitted(changeid))

  def testGetLatestSHA1ForBranch(self):
    """Verifies we can return the correct sha1 from mock data."""
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommandWithRetries')
    my_hash = 'sadfjaslfkj2135'
    my_branch = 'master'
    result = self.mox.CreateMock(cros_build_lib.CommandResult)
    result.returncode = 0
    result.output = '   '.join([my_hash, my_branch])
    cros_build_lib.RunCommandWithRetries(
        3, ['git', 'ls-remote',
            'ssh://gerrit.chromium.org:29418/tacos/chromite',
            'refs/heads/master'],
        redirect_stdout=True, print_cmd=True).AndReturn(result)
    self.mox.ReplayAll()
    helper = self._GetHelper()
    self.assertEqual(helper.GetLatestSHA1ForBranch('tacos/chromite',
                                                   my_branch), my_hash)
    self.mox.VerifyAll()

  def testGetLatestSHA1ForProject4Realz(self):
    """Verify we can check the latest hash from chromite."""
    helper = self._GetHelper()
    cros_build_lib.Info('The current sha1 on master for chromite is: %s' %
                        helper.GetLatestSHA1ForBranch('chromiumos/chromite',
                                                      'master'))

  def testSetReviewers(self):
    helper = self._GetHelper()
    # Ensure it requires at additions/removals.
    self.assertRaises(ValueError, helper.SetReviewers, 12345)
    def f(args):
      self.assertEqual(args[args.index('ferringb@chromium.org') -1], '--add')
      self.assertEqual(args[args.index('chrome-bot@blah') -1], '--remove')
      return True
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    cros_build_lib.RunCommand(mox.Func(f), redirect_stdout=True,
                              redirect_stderr=True)
    self.mox.ReplayAll()
    helper.SetReviewers(1, add=('ferringb@chromium.org',),
                        remove='chrome-bot@blah')
    self.mox.VerifyAll()


# pylint: disable=W0212,R0904
class GerritQueryTests(cros_test_lib.MoxTestCase):

  def setUp(self):
    raw_json = ('{"project":"chromiumos/chromite","branch":"master","id":'
             '"Icb8e1d315d465a077ffcddd7d1ab2307573017d5","number":"2144",'
             '"subject":"Add functionality to cbuildbot to patch in a set '
             'of Gerrit CL\u0027s","owner":{"name":"Ryan Cui","email":'
             '"rcui@chromium.org"},"url":'
             '"http://gerrit.chromium.org/gerrit/2144","lastUpdated":'
             '1307577655,"sortKey":"00158e2000000860","open":true,"status":'
             '"NEW","currentPatchSet":{"number":"3",'
             '"revision":"b1c82d0f1c916b7f66cfece625d67fb5ecea9ea7","ref":'
             '"refs/changes/44/2144/3","uploader":{"name":"Ryan Cui","email":'
             '"rcui@chromium.org"}}}')

    self.raw_json = raw_json
    self.good_footer = \
             '{"type":"stats","rowCount":1,"runTimeMilliseconds":4}'
    self.result = raw_json + '\n' + self.good_footer
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')

  def testPatchNotFound1(self):
    """Test case where ChangeID isn't found on internal server."""
    patches = ['Icb8e1d315d465a07']

    output_obj = cros_build_lib.CommandResult()
    output_obj.returncode = 0
    output_obj.output = ('{"type":"error",'
                         '"message":"Unsupported query:5S2D4D2D4"}')

    cros_build_lib.RunCommand(mox.In('gerrit.chromium.org'),
                              redirect_stdout=True).AndReturn(output_obj)

    self.mox.ReplayAll()

    self.assertRaises(gerrit.GerritException,
                      gerrit.GetGerritPatchInfo, patches)
    self.mox.VerifyAll()

  def _test_missing(self, patches):
    output_obj = cros_build_lib.CommandResult()
    output_obj.returncode = 0
    output_obj.output = '%s\n%s\n%s' % \
                        (self.raw_json, self.raw_json, self.good_footer)
    cros_build_lib.RunCommand(mox.In('gerrit.chromium.org'),
                              redirect_stdout=True).AndReturn(output_obj)

    self.mox.ReplayAll()

    self.assertRaises(gerrit.GerritException,
                      gerrit.GetGerritPatchInfo, patches)

    self.mox.VerifyAll()

  def testLooseQuery(self):
    """verify it complaints if an ID matches multiple"""
    self._test_missing(['Icb8e1d', 'ICeb8e1d3'])

  def testFirstNotFound(self):
    """verify it complains if the previous ID didn't match, but second did"""
    self._test_missing(['iab8e1d', 'iceb8e1d'])

  def testLastNotFound(self):
    """verify it complains if the last ID didn't match, but first did"""
    self._test_missing(['icb8e1d', 'iceb8e1de'])

  def testNumericFirstNotFound(self):
    """verify it complains if the previous numeric didn't match, but second
       did"""
    self._test_missing(['2144', '21445'])

  def testNumericLastNotFound(self):
    """verify it complains if the last numeric didn't match, but first did"""
    self._test_missing(['21445', '2144'])

  def _common_test(self, patches, server='gerrit.chromium.org',
    remote=constants.EXTERNAL_REMOTE, calls_allowed=1):

    output_obj = cros_build_lib.CommandResult()
    output_obj.returncode = 0
    output_obj.output = self.result

    for _ in xrange(calls_allowed):
      cros_build_lib.RunCommand(mox.In(server),
                                redirect_stdout=True).AndReturn(output_obj)

    self.mox.ReplayAll()

    patch_info = gerrit.GetGerritPatchInfo(patches)
    self.assertEquals(patch_info[0].remote, remote)
    self.mox.VerifyAll()
    return patch_info

  def testInternalID(self):
    self._common_test(['*Icb8e1d'], 'gerrit-int.chromium.org',
                      constants.INTERNAL_REMOTE)

  def testExternalID(self):
    self._common_test(['Icb8e1d'])

  def testExternalNumeric(self):
    self._common_test(['2144'])

  def testInternallNumeric(self):
    self._common_test(['*2144'], 'gerrit-int.chromium.org',
                      constants.INTERNAL_REMOTE)

  def testInternalUnique(self):
    self._common_test(['*2144', '*Icb8e1d'], 'gerrit-int.chromium.org',
                      constants.INTERNAL_REMOTE, calls_allowed=2)

  def testExternalUnique(self):
    """ensure that if two unique queries that point to the same cl, just one
       patch is returned"""
    self._common_test(['2144', 'Icb8e1d'], calls_allowed=2)

  def testPatchInfoParsing(self):
    """Test parsing of the JSON results."""
    patches = ['Icb8e1d315d465a07']

    output_obj = cros_build_lib.CommandResult()
    output_obj.returncode = 0
    output_obj.output = self.result

    cros_build_lib.RunCommand(mox.In('gerrit.chromium.org'),
                              redirect_stdout=True).AndReturn(output_obj)

    self.mox.ReplayAll()

    patch_info = gerrit.GetGerritPatchInfo(patches)
    self.assertEquals(patch_info[0].project, 'chromiumos/chromite')
    self.assertEquals(patch_info[0].ref, 'refs/changes/44/2144/3')

    self.mox.VerifyAll()


if __name__ == '__main__':
  cros_test_lib.main()
