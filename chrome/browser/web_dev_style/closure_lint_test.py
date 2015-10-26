#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import js_checker
from os import path as os_path
import re
from sys import path as sys_path
import unittest

_HERE = os_path.dirname(os_path.abspath(__file__))
sys_path.append(os_path.join(_HERE, '..', '..', '..', 'build'))

import find_depot_tools  # pylint: disable=W0611
from testing_support.super_mox import SuperMoxTestBase


class ClosureLintTest(SuperMoxTestBase):
  def setUp(self):
    SuperMoxTestBase.setUp(self)

    input_api = self.mox.CreateMockAnything()
    input_api.os_path = os_path
    input_api.re = re

    input_api.change = self.mox.CreateMockAnything()
    self.mox.StubOutWithMock(input_api.change, 'RepositoryRoot')
    src_root = os_path.join(os_path.dirname(__file__), '..', '..', '..')
    input_api.change.RepositoryRoot().MultipleTimes().AndReturn(src_root)

    output_api = self.mox.CreateMockAnything()

    self.mox.ReplayAll()

    self.checker = js_checker.JSChecker(input_api, output_api)

  def ShouldPassClosureLint(self, source):
    errors = self.checker.ClosureLint('', source=source)

    for error in errors:
      print 'Error: ' + error.message

    self.assertListEqual([], errors)

  def testBindFalsePositives(self):
    sources = [
      [
        'var addOne = function(prop) {\n',
        '  this[prop] += 1;\n',
        '}.bind(counter, timer);\n',
        '\n',
        'setInterval(addOne, 1000);\n',
        '\n',
      ],
      [
        '/** Da clickz. */\n',
        'button.onclick = function() { this.add_(this.total_); }.bind(this);\n',
      ],
    ]
    for source in sources:
      self.ShouldPassClosureLint(source)

  def testPromiseFalsePositives(self):
    sources = [
      [
        'Promise.reject(1).catch(function(error) {\n',
        '  alert(error);\n',
        '});\n',
      ],
      [
        'var loaded = new Promise();\n',
        'loaded.then(runAwesomeApp);\n',
        'loaded.catch(showSadFace);\n',
        '\n',
        '/** Da loadz. */\n',
        'document.onload = function() { loaded.resolve(); };\n',
        '\n',
        '/** Da errorz. */\n',
        'document.onerror = function() { loaded.reject(); };\n',
        '\n',
        "if (document.readystate == 'complete') loaded.resolve();\n",
      ],
    ]
    for source in sources:
      self.ShouldPassClosureLint(source)


if __name__ == '__main__':
  unittest.main()
