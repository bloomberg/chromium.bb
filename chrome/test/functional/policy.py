#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # must come before pyauto.
import pyauto


class PolicyTest(pyauto.PyUITest):
  """Tests that the effects of policies are being enforced as expected."""

  def IsBlocked(self, url):
    """Returns true if navigating to |url| is blocked."""
    self.NavigateToURL(url)
    blocked = self.GetActiveTabTitle() == url + ' is not available'
    ret = self.ExecuteJavascript("""
        var hasError = false;
        var error = document.getElementById('errorDetails');
        if (error) {
          hasError = error.textContent.indexOf('Error 138') == 0;
        }
        domAutomationController.send(hasError.toString());
    """)
    ret = ret == 'true'
    self.assertEqual(blocked, ret)
    return blocked

  def GetInnerHeight(self):
    """Returns the inner height of the content area."""
    ret = self.ExecuteJavascript(
        'domAutomationController.send(innerHeight.toString());');
    return int(ret)

  def testBlacklistPolicy(self):
    """Tests the URLBlacklist and URLWhitelist policies."""
    # This is an end to end test and not an exaustive test of the filter format.
    policy = {
      'URLBlacklist': [
        'news.google.com',
        'chromium.org',
      ],
      'URLWhitelist': [
        'dev.chromium.org',
        'chromium.org/chromium-os',
      ]
    }
    self.SetPolicies(policy)

    self.assertTrue(self.IsBlocked('http://news.google.com/'))
    self.assertFalse(self.IsBlocked('http://www.google.com/'))
    self.assertFalse(self.IsBlocked('http://google.com/'))

    self.assertTrue(self.IsBlocked('http://chromium.org/'))
    self.assertTrue(self.IsBlocked('http://www.chromium.org/'))
    self.assertFalse(self.IsBlocked('http://dev.chromium.org/'))
    self.assertFalse(self.IsBlocked('http://chromium.org/chromium-os/testing'))

  def testBookmarkBarPolicy(self):
    """Tests the BookmarkBarEnabled policy."""
    self.NavigateToURL('about:blank')
    self.assertFalse(self.GetBookmarkBarVisibility())
    self.assertFalse(self.IsBookmarkBarDetached())

    # It should be visible in detached state, in the NTP.
    self.NavigateToURL('chrome://newtab')
    self.assertFalse(self.GetBookmarkBarVisibility())
    self.assertTrue(self.IsBookmarkBarDetached())

    policy = {
      'BookmarkBarEnabled': True
    }
    self.SetPolicies(policy)

    self.assertTrue(self.WaitForBookmarkBarVisibilityChange(True))
    self.assertTrue(self.GetBookmarkBarVisibility())
    self.assertFalse(self.IsBookmarkBarDetached())
    # The accelerator should be disabled by the policy.
    self.ApplyAccelerator(pyauto.IDC_SHOW_BOOKMARK_BAR)
    self.assertTrue(self.WaitForBookmarkBarVisibilityChange(True))
    self.assertTrue(self.GetBookmarkBarVisibility())
    self.assertFalse(self.IsBookmarkBarDetached())

    policy['BookmarkBarEnabled'] = False
    self.SetPolicies(policy)

    self.assertTrue(self.WaitForBookmarkBarVisibilityChange(False))
    self.assertFalse(self.GetBookmarkBarVisibility())
    self.ApplyAccelerator(pyauto.IDC_SHOW_BOOKMARK_BAR)
    self.assertTrue(self.WaitForBookmarkBarVisibilityChange(False))
    self.assertFalse(self.GetBookmarkBarVisibility())
    # When disabled by policy, it should never be displayed at all,
    # not even on the NTP.
    self.assertFalse(self.IsBookmarkBarDetached())


if __name__ == '__main__':
  pyauto_functional.Main()
