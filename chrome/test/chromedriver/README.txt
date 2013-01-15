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

=====Architecture=====
ChromeDriver is shipped separately from Chrome. It controls Chrome out of
process through DevTools (WebKit Inspector). ChromeDriver is a shared library
which exports a few functions for executing WebDriver-standard commands, which
are packaged in JSON. The WebDriver open source project maintains clients in
various languages which eventually will be able to load this shared library.
There's a small patch that can be applied to the open source WebDriver Java
client to make it work with ChromeDriver.

For internal use, a custom python client for ChromeDriver is available in
chromedriver.py, which works for desktop (win/mac/linux).

ChromeDriver runs commands on the same thread that calls ExecuteCommand.
It doesn't create any additional threads except for the IO thread. The IO thread
is used to keep reading incoming data from Chrome in the background.

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
the OWNERS file in this directory. Issues are tracked in chromium's issue
tracker with Feature=WebDriver:
    https://code.google.com/p/chromium/issues/list?q=feature%3Dwebdriver
