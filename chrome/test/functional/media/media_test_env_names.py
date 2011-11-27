# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class MediaTestEnvNames(object):
  """Class that contains all environment names used in media tests.

  Since PyAuto does not support commandline arguments, we have to rely on
  environment variables. The following are the names of the environment
  variables that are used in chrome/src/test/functional/media_test_runner.py
  and media tests (subclasses of MediaTestBase in
  chrome/src/test/functional/media_test_base.py)
  """
  # PLAYER_HTML is a HTML file that contains media tag and other
  # JavaScript code for running the test.
  # Use this to indicate its URL.
  PLAYER_HTML_URL_ENV_NAME = 'PLAYER_HTML_URL'

  # Display the result output in compact form (e.g., "local", "remote").
  PLAYER_HTML_URL_NICKNAME_ENV_NAME = 'PLAYER_HTML_URL_NICKNAME'

  # Use this when you want to add extra information in the result output.
  EXTRA_NICKNAME_ENV_NAME = 'EXTRA_NICKNAME'

  # Define this environment variable when you do not want to report
  # the first result output. First result includes time to start up the
  # browser.
  REMOVE_FIRST_RESULT_ENV_NAME = 'REMOVE_FIRST_RESULT'

  # Add t=Data() parameter in query string to disable media cache
  # if this environment is defined.
  ADD_T_PARAMETER_ENV_NAME = 'ADD_T_PARAMETER'

  # Define the number of tries.
  N_RUNS_ENV_NAME = 'N_RUNS'

  # Define the tag name in HTML (either video or audio).
  MEDIA_TAG_ENV_NAME = 'HTML_TAG'

  # Define the media file name.
  MEDIA_FILENAME_ENV_NAME = 'MEDIA_FILENAME'

  # Define the media file nickname that is used for display.
  MEDIA_FILENAME_NICKNAME_ENV_NAME = 'MEDIA_FILENAME_NICKNAME'

  # Define the interval for the measurement.
  MEASURE_INTERVAL_ENV_NAME = 'MEASURE_INTERVALS'

  # Define the test scenario file, which contains all operations during tests.
  TEST_SCENARIO_FILE_ENV_NAME = 'TEST_SCENARIO_FILE'

  # Define the test scenario, which contains operations during tests.
  TEST_SCENARIO_ENV_NAME = 'TEST_SCENARIO'

  # Define this environment variable if we want to run test using binaries of
  # reference build, otherwise do not define this variable.
  REFERENCE_BUILD_ENV_NAME = 'REFERENCE_BUILD'

  # Define the path to the directory that contains binaries of reference build.
  REFERENCE_BUILD_DIR_ENV_NAME = 'REFERENCE_BUILD_DIR'

  # Define track(caption) file.
  TRACK_FILE_ENV_NAME = 'TRACK_FILE'

  # Define the number of additional players shown for stress testing.
  N_EXTRA_PLAYERS_ENV_NAME = 'N_EXTRA_PLAYERS'

  # Define this if this is jerky test.
  JERKY_TEST_ENV_NAME = 'JERKY_TEST'

  # Define location of the jerky tool binary.
  JERKY_TOOL_BINARY_LOCATION_ENV_NAME = 'JERKY_TOOL_LOC'

  # Define output directory of the jerky tool.
  JERKY_TOOL_OUTPUT_DIR_ENV_NAME = 'JERKY_TOOL_OUTPUT_DIR'

  # Define baseline directory of the jerky tool.
  JERKY_TOOL_BASELINE_DIR_ENV_NAME = 'JERKY_TOOL_BASELINE_DIR'
