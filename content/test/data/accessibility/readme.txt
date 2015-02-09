DumpAccessibilityTreeTest Notes

Files used:
* foo.html -- a file to be tested
* foo-expected-win.txt -- expected MSAA output
* foo-expected-mac.txt -- expected Mac accessibility output

Format for expected files:
* Blank lines and lines beginning with # are ignored
* Skipped files: if first line of file begins with #<skip then the entire file is ignored,
  but listed in the output as skipped
* Use 4 spaces for indent to show hierarchy
* MSAA states do not have a prefix, e.g. FOCUSED, not STATE_SYSTEM_FOCUSED
* All other constants are used exactly as normally named
* See specific examples (e.g. ul-expected-win.txt) for more details

Compiling and running the tests:
ninja -C out/Debug content_browsertests
out/Debug/content_browsertests --gtest_filter="DumpAccessibilityTreeTest*"

If you are adding a new test file remember to add a corresponding test case in
content/browser/accessibility/dump_accessibility_events_browsertest.cc
or
content/browser/accessibility/dump_accessibility_tree_browsertest.cc
