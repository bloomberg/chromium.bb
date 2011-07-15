#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import pyauto


class EnterprisePolicyTest(pyauto.PyUITest):
  """Test for enterprise policy.

  The device is expected to be manually enrolled and logged in as an
  enterprise user.
  """


  def testPolicy(self):
    self.assertTrue(self.IsEnterpriseDevice())
    self.FetchEnterprisePolicy()


if __name__ == '__main__':
  pyauto_functional.Main()
