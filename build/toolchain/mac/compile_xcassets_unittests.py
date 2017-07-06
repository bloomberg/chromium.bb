# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import compile_xcassets


class TestFilterCompilerOutput(unittest.TestCase):

  relative_paths = {
    '/Users/janedoe/chromium/src/Chromium.xcassets':
        '../../Chromium.xcassets',
    '/Users/janedoe/chromium/src/out/Default/Chromium.app/Assets.car':
        'Chromium.app/Assets.car',
  }

  def testNoError(self):
    self.assertEquals(
        '',
        compile_xcassets.FilterCompilerOutput(
            '/* com.apple.actool.compilation-results */\n'
            '/Users/janedoe/chromium/src/out/Default/Chromium.app/Assets.car\n',
            self.relative_paths))

  def testNoErrorRandomMessages(self):
    self.assertEquals(
        '',
        compile_xcassets.FilterCompilerOutput(
            '2017-07-04 04:59:19.460 ibtoold[23487:41214] CoreSimulator is att'
                'empting to unload a stale CoreSimulatorService job.  Existing'
                ' job (com.apple.CoreSimulator.CoreSimulatorService.179.1.E8tt'
                'yeDeVgWK) is from an older version and is being removed to pr'
                'event problems.\n'
            '/* com.apple.actool.compilation-results */\n'
            '/Users/janedoe/chromium/src/out/Default/Chromium.app/Assets.car\n',
            self.relative_paths))

  def testWarning(self):
    self.assertEquals(
        '/* com.apple.actool.document.warnings */\n'
        '../../Chromium.xcassets:./image1.imageset/[universal][][][1x][][][]['
            '][][]: warning: The file "image1.png" for the image set "image1"'
            ' does not exist.\n',
        compile_xcassets.FilterCompilerOutput(
            '/* com.apple.actool.document.warnings */\n'
            '/Users/janedoe/chromium/src/Chromium.xcassets:./image1.imageset/['
                'universal][][][1x][][][][][][]: warning: The file "image1.png'
                '" for the image set "image1" does not exist.\n'
            '/* com.apple.actool.compilation-results */\n'
            '/Users/janedoe/chromium/src/out/Default/Chromium.app/Assets.car\n',
            self.relative_paths))

  def testError(self):
    self.assertEquals(
        '/* com.apple.actool.errors */\n'
        '../../Chromium.xcassets: error: The output directory "/Users/janedoe/'
            'chromium/src/out/Default/Chromium.app" does not exist.\n',
        compile_xcassets.FilterCompilerOutput(
            '/* com.apple.actool.errors */\n'
            '/Users/janedoe/chromium/src/Chromium.xcassets: error: The output '
                'directory "/Users/janedoe/chromium/src/out/Default/Chromium.a'
                'pp" does not exist.\n'
            '/* com.apple.actool.compilation-results */\n',
            self.relative_paths))


if __name__ == '__main__':
  unittest.main()
