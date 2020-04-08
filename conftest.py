# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration and fixtures for pytest.

See the following doc link for an explanation of conftest.py and how it is used
by pytest:
https://docs.pytest.org/en/latest/fixture.html#conftest-py-sharing-fixture-functions
"""

from __future__ import print_function

import multiprocessing

import pytest  # pylint: disable=import-error

from chromite.lib import cidb


@pytest.fixture(scope='class', autouse=True)
def mock_cidb_connection():
  """Ensure that the CIDB connection factory is initialized as a mock.

  Unit tests should never connect to any live instances of CIDB and this
  initialization ensures that they only ever get a mock connection instance.

  Previously cros_test_lib.TestProgram.runTests was responsible for globally
  initializing this mock and multiple tests are flaky if this mock connection
  is not initialized before any tests are run.
  """
  # pylint: disable=protected-access
  cidb.CIDBConnectionFactory._ClearCIDBSetup()
  cidb.CIDBConnectionFactory.SetupMockCidb()


@pytest.fixture(scope='class', autouse=True)
def assert_no_zombies():
  """Assert that tests have no active child processes after completion.

  This assertion runs after class tearDown methods because of the scope='class'
  declaration.
  """
  yield
  children = multiprocessing.active_children()
  if children:
    pytest.fail('Test has %s active child processes after tearDown: %s' %
                (len(children), children))


@pytest.fixture
def legacy_capture_output(request, capfd):
  """Adds the `capfd` fixture to TestCase-style test classes.

  This fixture should only be used on cros_test_lib.TestCase test classes, since
  it doesn't yield anything and just attaches the built-in pytest `capfd`
  fixture to the requesting class. Tests written as standalone functions should
  use pytest's built-in `capfd` fixture instead of this. See the documentation
  for more information on how to use the `capfd` fixture that this provides:
  https://docs.pytest.org/en/latest/reference.html#capfd

  See the following documentation for an explanation of why fixtures have to be
  provided to TestCase classes in this manner:
  https://docs.pytest.org/en/latest/unittest.html#mixing-pytest-fixtures-into-unittest-testcase-subclasses-using-marks
  """
  request.cls.capfd = capfd
