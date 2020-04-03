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

import pytest # pylint: disable=import-error

from chromite.lib import cidb


@pytest.fixture(scope='session', autouse=True)
def mock_cidb_connection():
  """Ensure that the CIDB connection factory is initialized as a mock.

  Unit tests should never connect to any live instances of CIDB and this
  initialization ensures that they only ever get a mock connection instance.

  Previously cros_test_lib.TestProgram.runTests was responsible for globally
  initializing this mock and multiple tests are flaky if this mock connection
  is not initialized before any tests are run.
  """
  cidb.CIDBConnectionFactory.SetupMockCidb()
