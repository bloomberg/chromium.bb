This file contains high-level info about how ChromeDriver works and how to
contribute.

ChromeDriver is an implementation of the WebDriver standard,
which allows users to automate testing of their website across browsers.

=====Getting started=====
Build ChromeDriver by building the 'chromedriver2' target. This will create
a shared library in the build folder named 'chromedriver2.dll' (win),
'chromedriver2.so' (mac), or 'libchromedriver2.so' (linux).

Once built, ChromeDriver can be used interactively with python.
This can be easily done by running python in this directory (or including
this directory in your PYTHONPATH).

$ python
>>> import chromedriver
>>> driver = chromedriver.ChromeDriver('/path/to/chromedriver2/library')
>>> driver.Load('http://www.google.com')

ChromeDriver will use the system installed Chrome by default.

To use ChromeDriver2 with Chrome on Android pass the Android package name in the
chromeOptions.androidPackage capability when creating the driver. The path to
adb_commands.py and the adb tool from the Android SDK must be set in PATH. For
more detailed instructions see the wiki:
    https://code.google.com/p/chromedriver/wiki/ChromeDriver2forAndroid

NOTE: on 64-bit OSX machines (most modern ones, including laptops) it is
necessary to set the environment variable VERSIONER_PYTHON_PREFER_32_BIT=yes,
because the 'chromedriver2.so' library is 32-bit, while on 64-bit OSX machines
(most modern ones including laptops), python starts as a 64-bit binary by
default and would not be able to load the library.

=====Architecture=====
ChromeDriver is shipped separately from Chrome. It controls Chrome out of
process through DevTools (WebKit Inspector). ChromeDriver is a shared library
which exports a few functions for executing WebDriver-standard commands, which
are packaged in JSON. For internal use, a custom python client for ChromeDriver
is available in chromedriver.py, which works on desktop (win/mac/linux) with
the shared library ChromeDriver.

The ChromeDriver shared library runs commands on the same thread that calls
ExecuteCommand. It doesn't create any additional threads except for the IO
thread. The IO thread is used to keep reading incoming data from Chrome in the
background.

ChromeDriver is also available as a standalone server executable which
communicates via the WebDriver JSON wire protocol. This can be used with the
open source WebDriver client libraries.

=====Code structure=====
Code under the 'chrome' subdirectory is intended to be unaware of WebDriver and
serve as a basic C++ interface for controlling Chrome remotely via DevTools.
As such, it should not have any WebDriver-related dependencies.

1) chrome/test/chromedriver
Implements chromedriver commands.

2) chrome/test/chromedriver/chrome
Code to deal with chrome specific stuff, like starting Chrome on different OS
platforms, controlling Chrome via DevTools, handling events from DevTools, etc.

3) chrome/test/chromedriver/js
Javascript helper scripts.

4) chrome/test/chromedriver/net
Code to deal with network communication, such as connection to DevTools.

5) chrome/test/chromedriver/server
Code for the chromedriver server.

6) chrome/test/chromedriver/third_party
Third party libraries used by chromedriver.

=====Testing=====
There are 4 test suites for verifying ChromeDriver's correctness:

1) chromedriver2_unittests (chrome/chrome_tests.gypi)
This is the unittest target, which runs on the main waterfall on win/mac/linux
and can close the tree. It is also run on the commit queue and try bots by
default. Tests should take a few milliseconds and be very stable.

2) chromedriver2_tests (chrome/chrome_tests.gypi)
This is a collection of C++ medium sized tests which can be run optionally
on the trybots.

3) python tests
These are integration tests which can be found in run_py_tests.py. They are
run on the chromium QA bots:
    http://build.chromium.org/p/chromium.pyauto/waterfall

4) WebDriver Java acceptance tests
These are integration tests from the WebDriver open source project which can
be run via run_java_tests.py. They are also run on the chromium QA bots.
See http://src.chromium.org/viewvc/chrome/trunk/deps/third_party/webdriver

=====Contributing=====
Find an open issue and submit a patch for review by an individual listed in
the OWNERS file in this directory. Issues are tracked in chromedriver's issue
tracker:
    https://code.google.com/p/chromedriver/issues/list
