This file contains high-level info about how ChromeDriver works and how to
contribute.

=====Architecture=====
ChromeDriver is an implementation of the WebDriver standard,
which allows users to automate testing of their website across browsers.

ChromeDriver is shipped separately from Chrome. It controls Chrome out of
process through DevTools (WebKit Inspector). ChromeDriver is a shared library
which exports a few functions for executing WebDriver-standard commands, which
are packaged in JSON. The WebDriver open source project maintains clients in
various languages which can load this shared library.

For internal use, a custom python client for ChromeDriver is available in
chromedriver.py, which works for desktop (win/mac/linux).

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

=====Issues=====
Issues are tracked in chromium's issue tracker with Feature=WebDriver:
    https://code.google.com/p/chromium/issues/list?q=feature%3Dwebdriver
