# Code coverage in Chromium

## Coverage script.
[Coverage script] helps to generate clang source-based code coverage report
locally and make it easy to identify under-covered areas.

Currently, this script supports running on Linux, Mac, iOS and ChromeOS
platforms, and supports displaying code coverage results per directory, per
component and per file.

In order to generate code coverage report, you need to first add
`use_clang_coverage=true` and `is_component_build=false` GN flags to args.gn
file in your build output directory (e.g. out/coverage).

Existing implementation requires `is_component_build=false` flag
because coverage info for dynamic libraries may be missing and
`is_component_build` is set to true by `is_debug` unless it is explicitly set
to false.

Example usage:
```
gn gen out/coverage --args='use_clang_coverage=true is_component_build=false'
gclient runhooks
python tools/code_coverage/coverage.py crypto_unittests url_unittests
      -b out/coverage -o out/report -c 'out/coverage/crypto_unittests'
      -c 'out/coverage/url_unittests --gtest_filter=URLParser.PathURL'
      -f url/ -f crypto/
```

For more options, please refer to the script.

## Reporting problems

For any breakage report and feature requests, please [file a bug].

[Coverage script]: https://cs.chromium.org/chromium/src/tools/code_coverage/coverage.py
[file a bug]: https://bugs.chromium.org/p/chromium/issues/entry?components=Tools%3ECodeCoverage